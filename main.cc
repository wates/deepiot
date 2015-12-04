#include <uv.h>
#include <memory.h>
#include <string>
#include <map>
#include <iostream>
#include <vector>
#include <sstream>
#include <functional>
#include <algorithm>
#include <regex>
#include <map>

#include "http-parser/http_parser.h"
#include "drivers/tenkey.h"

const int httpd_port = 80;

namespace{
  uv_loop_t uv;
  uv_tcp_t httpd;
  bool halt = false;
  Tenkey *tenkey;
}

typedef std::vector <std::pair< std::string, std::function<std::string(std::string)> > > path_map;
path_map get;

int main()
{
  using namespace std;
  tenkey = Tenkey::Create();
  uv_loop_init(&uv);
  uv_timer_t poller;
  uv_timer_init(&uv, &poller);
  uv_timer_start(&poller, [](uv_timer_t*poller){
    if (tenkey)
    {
      tenkey->Update();
      for (auto i : tenkey->GetPushState()){
        cout << (int)i << endl;
      }
    }
    if (halt){
      uv_timer_stop(poller);
      uv_close(reinterpret_cast<uv_handle_t*>(&httpd), [](uv_handle_t*){});
    }
  }, 0, 1);
  sockaddr_in addr;
  uv_ip4_addr("0.0.0.0", httpd_port, &addr);
  if (uv_tcp_init(&uv, &httpd) ||
    uv_tcp_bind(&httpd, (const struct sockaddr*) &addr, 0) ||
    uv_tcp_simultaneous_accepts((uv_tcp_t*)&httpd, 1) ||
    uv_listen((uv_stream_t*)&httpd, SOMAXCONN, [](uv_stream_t* listener, int status){
    uv_stream_t *stream = new uv_stream_t;
    if (uv_tcp_init(&uv, reinterpret_cast<uv_tcp_t*>(stream)) ||
      uv_tcp_simultaneous_accepts(reinterpret_cast<uv_tcp_t*>(stream), 1) ||
      uv_accept(listener, stream) ||
      uv_tcp_nodelay(reinterpret_cast<uv_tcp_t*>(stream), 1) ||
      uv_read_start(stream, [](uv_handle_t*, size_t suggested_size, uv_buf_t* buf){
      buf->base = new char[suggested_size];
      buf->len = suggested_size;
    }, [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf){
      if (nread < 0) { //error
        uv_close(reinterpret_cast<uv_handle_t*>(stream), [](uv_handle_t* handle){
        });
      }
      else if (0 == nread){ //remote close
      }
      else{ //good
        static const http_parser_settings settings = {
          NULL, [](http_parser *parser, const char *at, size_t length)->int{ //url
            auto stream = reinterpret_cast<uv_stream_t*>(parser->data);
            const string path(at, at + length);
            stringstream res;
            res << "HTTP/1.1 200 OK\r\n"
              "Connection: close\r\n"
              "\r\n";
            if (std::none_of(::get.begin(), ::get.end(), [&](decltype(*::get.end()) &i){
              if (0 == path.find(i.first)){
                res << i.second(path);
                return true;
              }
              return false;
            })){
            }
            uv_write_t *write_req = new uv_write_t;
            uv_buf_t buf;
            buf.len = res.str().length();
            buf.base = new char[buf.len];
            write_req->data = buf.base;
            memcpy(buf.base, res.str().data(), buf.len);
            uv_write(write_req, stream, &buf, 1, [](uv_write_t *write_req, int status){
              delete[]reinterpret_cast<char*>(write_req->data);
              uv_shutdown_t *st = new uv_shutdown_t;
              uv_shutdown(st, write_req->handle, [](uv_shutdown_t *st, int status){
                uv_close(reinterpret_cast<uv_handle_t*>(st->handle), [](uv_handle_t* handle){
                  //delete reinterpret_cast<uv_stream_t*>(handle);
                });
                delete st;
              });
              delete write_req;
            });
            return length;
          }, NULL, NULL, NULL, NULL, NULL, NULL
        };
        http_parser_execute(reinterpret_cast<http_parser*>(stream->data), &settings, buf->base, nread);
      }
      delete[]buf->base;
    }))
    {
      //setup error
      uv_close(reinterpret_cast<uv_handle_t*>(stream), [](uv_handle_t* handle){
      });
    }
    else{
      http_parser *parser = new http_parser;
      http_parser_init(parser, HTTP_REQUEST);
      stream->data = parser;
      parser->data = stream;
    }
  }))
  {
    cout << "listen error" << endl;;
    return -1;
  }

  ::get.push_back({"/tenkey/on", [](const string &path){
    std::stringstream ss;
    if (tenkey){
      for (auto i : tenkey->GetPushState()){
        ss << (int)i << " ";
      }
    }
    return ss.str();
  } });

  ::get.push_back({ "/tenkey", [](const string &path){
    return "/tenkey/on\r\n"
      "/tenkey/push\r\n";
  } });

  ::get.push_back({ "/", [](const string &path){
    return "/tenkey\r\n";
  } });

  uv_run(&uv, UV_RUN_DEFAULT);
  return 0;
}
