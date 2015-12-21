#include "sdr.h"
#include "musen/rtl-sdr.h"
#include <thread>
#include <mutex>
class SdrImpl
  :public Sdr
{
public:
  SdrImpl()
  {
  }
  ~SdrImpl(){
    if (loop_){
      endthread_ = true;
      loop_->join();
      delete loop_;
    }
  }
  bool Init(){
    rtlsdr_open(&dev_, 0);
    int gains[100];
    int count = rtlsdr_get_tuner_gains(dev_, NULL);
    fprintf(stderr, "Supported gain values (%d): ", count);
    count = rtlsdr_get_tuner_gains(dev_, gains);
    for (int i = 0; i < count; i++)
      fprintf(stderr, "%.1fdb ", gains[i] / 10.0);
    fprintf(stderr, "\n");

    static uint32_t samp_rate = 2048000;

    rtlsdr_set_sample_rate(dev_, samp_rate);
    Tune(90.5f);
    rtlsdr_set_tuner_gain_mode(dev_, 0);
    rtlsdr_reset_buffer(dev_);
    loop_ = new std::thread([this](){
      uint8_t buffer[512 * 32];
      int sum_read = 0;
      while (!endthread_){
        int n_read = 0;
        rtlsdr_read_sync(dev_, buffer + sum_read, sizeof(buffer) - sum_read, &n_read);
        sum_read += n_read;
        if (sizeof(buffer) - 512 < sum_read)
        {
          std::lock_guard<std::mutex> lk(m_);
          deq_.insert(deq_.end(), buffer, buffer + sum_read);
          if (10 * 1024 * 1024 < deq_.size()){
            deq_.erase(deq_.begin(), deq_.begin() + deq_.size() - 10 * 1024 * 1024);
          }
          sum_read = 0;
        }

#if 1
        static int speed = 0;
        static time_t t = time(0);
        speed += n_read;
        if (t != time(0)){
          printf("%d bytes/sec %d pkt/sec\n", speed, speed / 512);
          t = time(0);
          speed = 0;
        }
#endif
      }
      rtlsdr_close(dev_);
    });
    return true;
  }
  uint32_t Tune(float Mhz)
  {
    rtlsdr_set_center_freq(dev_, Mhz * 1000 * 1000);
    return rtlsdr_get_center_freq(dev_);
  }
  void Peek(uint8_t *sample, int length)
  {
    std::lock_guard<std::mutex> lk(m_);
    if (length < deq_.size()){
      for (size_t i = 0; i < length; i++){
        sample[i] = deq_[deq_.size() - length + i];
      }
    }
  }
  void Spectrum(float *sample, int length, int fold)
  {
    const int AMP = fold;
    const int L = length*AMP * 2;
    uint8_t *wave = new uint8_t[L];
    Peek(wave, L);
    float *ar = new float[L], *ai = new float[L];
    memset(ai, 0, sizeof(int)*L);
    memset(sample, 0, sizeof(float)*length);
    for (int j = 0; j < L; j++){
      ar[j] = (float)wave[j] - 128;
    }
    fft(L, ar, ai);
    for (int j = 0; j < L / 2; j++){
      sample[j / AMP] += sqrtf(ar[j] * ar[j] + ai[j] * ai[j]);
    }
    for (int j = 0; j < L / 2 / AMP; j++){
      sample[j] /= AMP;
    }

    delete[]ar, delete[]ai;
    delete[]wave;
  }

  template<typename T>void fft(int n, T *ar, T *ai)
  {
    int m, mq, irev, i, j, j1, j2, j3, k;
    float theta, w1r, w1i, w3r, w3i;
    float x0r, x0i, x1r, x1i, x3r, x3i;
    i = 0;
    for (j = 1; j < n - 1; j++) {
      for (k = n >> 1; k > (i ^= k); k >>= 1);
      if (j < i) {
        x0r = ar[j];
        x0i = ai[j];
        ar[j] = ar[i];
        ai[j] = ai[i];
        ar[i] = x0r;
        ai[i] = x0i;
      }
    }
    theta = -2 * atanf(1.0) / n;
    for (m = 4; m <= n; m <<= 1) {
      mq = m >> 2;
      for (k = mq; k >= 1; k >>= 2) {
        for (j = mq - k; j < mq - (k >> 1); j++) {
          j1 = j + mq;
          j2 = j1 + mq;
          j3 = j2 + mq;
          x1r = ar[j] - ar[j1];
          x1i = ai[j] - ai[j1];
          ar[j] += ar[j1];
          ai[j] += ai[j1];
          x3r = ar[j3] - ar[j2];
          x3i = ai[j3] - ai[j2];
          ar[j2] += ar[j3];
          ai[j2] += ai[j3];
          ar[j1] = x1r - x3i;
          ai[j1] = x1i + x3r;
          ar[j3] = x1r + x3i;
          ai[j3] = x1i - x3r;
        }
      }
      if (m == n) continue;
      irev = n >> 1;
      w1r = cosf(theta * irev);
      for (k = mq; k >= 1; k >>= 2) {
        for (j = m + mq - k; j < m + mq - (k >> 1); j++) {
          j1 = j + mq;
          j2 = j1 + mq;
          j3 = j2 + mq;
          x1r = ar[j] - ar[j1];
          x1i = ai[j] - ai[j1];
          ar[j] += ar[j1];
          ai[j] += ai[j1];
          x3r = ar[j3] - ar[j2];
          x3i = ai[j3] - ai[j2];
          ar[j2] += ar[j3];
          ai[j2] += ai[j3];
          x0r = x1r - x3i;
          x0i = x1i + x3r;
          ar[j1] = w1r * (x0r + x0i);
          ai[j1] = w1r * (x0i - x0r);
          x0r = x1r + x3i;
          x0i = x1i - x3r;
          ar[j3] = w1r * (-x0r + x0i);
          ai[j3] = w1r * (-x0i - x0r);
        }
      }
      for (i = 2 * m; i < n; i += m) {
        for (k = n >> 1; k > (irev ^= k); k >>= 1);
        w1r = cosf(theta * irev);
        w1i = sinf(theta * irev);
        w3r = cosf(theta * 3 * irev);
        w3i = sinf(theta * 3 * irev);
        for (k = mq; k >= 1; k >>= 2) {
          for (j = i + mq - k; j < i + mq - (k >> 1); j++) {
            j1 = j + mq;
            j2 = j1 + mq;
            j3 = j2 + mq;
            x1r = ar[j] - ar[j1];
            x1i = ai[j] - ai[j1];
            ar[j] += ar[j1];
            ai[j] += ai[j1];
            x3r = ar[j3] - ar[j2];
            x3i = ai[j3] - ai[j2];
            ar[j2] += ar[j3];
            ai[j2] += ai[j3];
            x0r = x1r - x3i;
            x0i = x1i + x3r;
            ar[j1] = w1r * x0r - w1i * x0i;
            ai[j1] = w1r * x0i + w1i * x0r;
            x0r = x1r + x3i;
            x0i = x1i - x3r;
            ar[j3] = w3r * x0r - w3i * x0i;
            ai[j3] = w3r * x0i + w3i * x0r;
          }
        }
      }
    }
    mq = n >> 1;
    for (k = mq; k >= 1; k >>= 2) {
      for (j = mq - k; j < mq - (k >> 1); j++) {
        j1 = mq + j;
        x0r = ar[j] - ar[j1];
        x0i = ai[j] - ai[j1];
        ar[j] += ar[j1];
        ai[j] += ai[j1];
        ar[j1] = x0r;
        ai[j1] = x0i;
      }
    }
  }


private:
  volatile bool running = false;
  volatile bool endthread_ = false;
  std::vector<uint8_t> deq_;
  volatile int tune_hz = 0;
  rtlsdr_dev_t *dev_ = nullptr;
  std::thread *loop_ = nullptr;
  std::mutex m_;
};

Sdr* Sdr::Create()
{
  SdrImpl *sdr = new SdrImpl();
  if (sdr->Init()){
    return sdr;
  }
  delete sdr;
  return nullptr;
}
