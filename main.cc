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
#include "drivers/webcam.h"
#include "png/png.h"
#include "drivers/sdr.h"
#include "drivers/rokujiku.h"
#include "drivers/osemb.h"

const int httpd_port = 80;

using namespace std;

uv_loop_t uv;
uv_tcp_t httpd;
bool halt = false;
Tenkey *tenkey;
Webcam *webcam;
Sdr *sdr;
Rokujiku *rokujiku;
Os *os;
uv_timer_t poller, long_timer;

struct HttpBody
{
  string header;
  vector<uint8_t> body;
  HttpBody(const string &in){
    body.assign(in.begin(), in.end());
    header += "Content-Type: text/plain\r\n";
  }
  HttpBody(const vector<uint8_t> &in){
    body.assign(in.begin(), in.end());
  }
  HttpBody(){}
};

typedef vector <pair< string, function<HttpBody(string)> > > path_map;
path_map get;

template<typename A, typename B> A round(B b){
  return b < 0 ? 0 : (sizeof(A) * 256 <= b) ? sizeof(A) * 256 - 1 : b;
}

int main()
{
  tenkey = Tenkey::Create();
  webcam = Webcam::Create();
  if (webcam){
    webcam->Start();
  }
  sdr = Sdr::Create();
  rokujiku = Rokujiku::Create();
  os = Os::Create();
  uv_loop_init(&uv);
  uv_timer_init(&uv, &poller);
  uv_timer_init(&uv, &long_timer);
  //short time loop
  uv_timer_start(&poller, [](uv_timer_t*timer){
    if (tenkey)
    {
      tenkey->Update();
      //for (auto i : tenkey->GetPushState()){
      //  cout << (int)i << endl;
      //}
    }
    if (rokujiku)
    {
      rokujiku->Update();
    }
    if (os){
      os->Poll();
    }
    if (halt){
      uv_timer_stop(&poller);
      uv_timer_stop(&long_timer);
      uv_close(reinterpret_cast<uv_handle_t*>(&httpd), [](uv_handle_t*){});
    }
  }, 0, 10);

  //long timer
  uv_timer_start(&long_timer, [](uv_timer_t*timer){
    if (webcam)
    {
      webcam->Stop();
      webcam->Start();
    }
    if (rokujiku)
    {
      rokujiku->Activate();

    }
  }, 0, 1000 * 60 * 60);

  //httpd
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
          delete handle;
        });
      }
      else if (0 == nread){ //remote close
      }
      else{ //good
        static const http_parser_settings settings = {
          NULL, [](http_parser *parser, const char *at, size_t length)->int{ //url
            auto stream = reinterpret_cast<uv_stream_t*>(parser->data);
            const string path(at, at + length);
            vector<uint8_t> res;
            static const char base_header[] =
              "HTTP/1.1 200 OK\r\n"
              "Connection: close\r\n";
            res.insert(res.begin(), base_header, base_header + strlen(base_header));
            if (none_of(::get.begin(), ::get.end(), [&](decltype(*::get.end()) &i){
              if (0 == path.find(i.first)){
                HttpBody b = i.second(path);
                b.header += "Content-Length: " + std::to_string(b.body.size());
                b.header += "\r\n\r\n";
                res.insert(res.end(), b.header.begin(), b.header.end());
                res.insert(res.end(), b.body.begin(), b.body.end());
                return true;
              }
              return false;
            })){
            }
            uv_write_t *write_req = new uv_write_t;
            uv_buf_t buf;
            buf.len = res.size();
            buf.base = new char[buf.len];
            write_req->data = buf.base;
            memcpy(buf.base, res.data(), buf.len);//TODO: copiful
            uv_write(write_req, stream, &buf, 1, [](uv_write_t *write_req, int status){
              delete[]reinterpret_cast<char*>(write_req->data);
              uv_shutdown_t *st = new uv_shutdown_t;
              uv_shutdown(st, write_req->handle, [](uv_shutdown_t *st, int status){
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

  //tenkey

  ::get.push_back({ "/tenkey/on", [](const string &path){
    stringstream ss;
    if (tenkey){
      for (auto i : tenkey->GetPushState()){
        ss << (int)i << " ";
      }
    }
    ss << "\r\n";
    return ss.str();
  } });

  ::get.push_back({ "/tenkey/ascii", [](const string &path){
    stringstream ss;
    if (tenkey){
      for (auto i : tenkey->Ascii()){
        ss << i;
      }
    }
    ss << "\r\n";
    return ss.str();
  } });

  ::get.push_back({ "/tenkey", [](const string &path){
    return "/tenkey/on\r\n"
      "/tenkey/ascii\r\n";
  } });

  //webcam

  ::get.push_back({ "/webcam/width", [](const string &path){
    return webcam ? std::to_string(webcam->Width()) : "-1";
  } });

  ::get.push_back({ "/webcam/height", [](const string &path){
    return webcam ? std::to_string(webcam->Height()) : "-1";
  } });

  ::get.push_back({ "/webcam/capture.png", [](const string &path)->HttpBody{
    if (nullptr == webcam)
      return "";

    HttpBody b;
    b.header += "Content-Type: image/png\r\n";

    wts::Raw raw;
    wts::WriteFromR8G8B8(&raw, webcam->Width(), webcam->Height(), webcam->Buffer());
    b.body.assign(raw.data, raw.data + raw.size);
    wts::FreeRaw(&raw);

    return b;
  } });

  ::get.push_back({ "/webcam", [](const string &path){
    return "/webcam/width\r\n"
      "/webcam/height\r\n"
      "/webcam/capture.png\r\n"
      ;
  } });

  //sdr

  ::get.push_back({ "/sdr/tune/", [](const string &path){
    std::smatch m;
    return std::regex_search(path, m, std::regex("[0-9\\.]+")) ?
      std::to_string(sdr->Tune(std::stof(m[0]))) : std::string("/sdr/tune/%f [Mhz]\r\n");
  } });

  ::get.push_back({ "/sdr/buffer", [](const string &path){
    stringstream ss;
    if (sdr){
      std::array<uint8_t, 256> buf;
      sdr->Peek(&buf.front(), buf.size());
      if (sdr){
        for (int i : buf){
          ss << i - 128 << " ";
        }
      }
    }
    ss << "\r\n";
    return ss.str();
  } });

  ::get.push_back({ "/sdr/spectrum.png", [](const string &path){
    HttpBody b;
    if (nullptr == sdr){
      return b;
    }
    static const int kSample = 1024;
    static const int kHeight = 256;
    std::array<float, kSample> buf;
    sdr->Spectrum(&buf.front(), buf.size());
    b.header += "Content-Type: image/png\r\n";

    std::array<uint8_t, kSample * kHeight * 3> rgb;

    for (auto &i : rgb)i = 0;
    float m = 0;
    for (auto &n : buf)if (m < n)m = n;
    for (auto &n : buf)n /= m / (kHeight - 1);

    for (int y = 0; y < kHeight; y++)
    {
      for (int x = 0; x < kSample; x++)
      {
        uint8_t *p = &*rgb.begin() + (y*kSample + x) * 3;
        if (256 - buf[x] < y)
        {
          float h = (buf[x] / 1.5f) / 256.0f + 0.03f;
          int v = 255;
          int i = (int)(h * 6);
          float f = h * 6 - i;
          int q = 255 * (1 - f);
          int t = 255 * f;
          switch (i)
          {
          case 0: p[0] = v, p[1] = t, p[2] = 0; break;
          case 1: p[0] = q, p[1] = v, p[2] = 0; break;
          case 2: p[0] = 0, p[1] = v, p[2] = t; break;
          case 3: p[0] = 0, p[1] = q, p[2] = v; break;
          case 4: p[0] = t, p[1] = 0, p[2] = v; break;
          default:p[0] = v, p[1] = 0, p[2] = q; break;
          }
        }
      }
    }

    wts::Raw raw;
    wts::WriteFromR8G8B8(&raw, kSample, kHeight, &*rgb.begin());
    b.body.assign(raw.data, raw.data + raw.size);
    wts::FreeRaw(&raw);
    return b;
  } });

  ::get.push_back({ "/sdr/spectrum", [](const string &path){
    stringstream ss;
    if (sdr){
      std::array<float, 1024> buf;
      sdr->Spectrum(&buf.front(), buf.size());
      if (sdr){
        for (auto i : buf){
          ss << i - 128 << " ";
        }
      }
      ss << "\r\n";
    }
    return ss.str();
  } });

  ::get.push_back({ "/sdr", [](const string &path){
    return "/sdr/tune/%f [Mhz]\r\n"
      "/sdr/buffer\r\n"
      "/sdr/spectrum\r\n"
      ;
  } });

  // os

  ::get.push_back({ "/display/white", [](const string &path){
    os->Flash(0x00ffffff);
    return "ok";
  } });

  ::get.push_back({ "/display/black", [](const string &path){
    os->Flash(0x00000000);
    return "ok";
  } });

  ::get.push_back({ "/display/", [](const string &path){
    std::smatch m;
    if (std::regex_search(path, m, std::regex("[0-9a-fA-F]{6}$")))
      os->Flash(std::stoi(m[0], nullptr, 16));
    return m.str();
  } });

  ::get.push_back({ "/display", [](const string &path){
    return "/display/white\r\n"
      "/display/black\r\n"
      "/display/RRGGBB\r\n"
      ;
  } });

  //root

  ::get.push_back({ "/", [](const string &path){
    return "/tenkey\r\n"
      "/webcam\r\n"
      "/sdr\r\n"
      "/display\r\n"
      ;
  } });

  uv_run(&uv, UV_RUN_DEFAULT);

  if (webcam){
    webcam->Release();
    delete webcam;
  }
  if (tenkey){
    delete tenkey;
  }
  return 0;
}
