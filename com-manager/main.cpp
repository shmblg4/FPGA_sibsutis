#include <iostream>
#include <windows.h>
#include <mmsystem.h>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>

#define SAMPLE_RATE 24000
#define BUFFER_SIZE 4096
#define NUM_BUFFERS 2
#define DELAY_SECONDS 5

std::atomic<bool> stopFlag(false);

// Кольцевой буфер для передачи данных
class CircularBuffer
{
private:
    std::vector<std::vector<short>> buffer;
    size_t capacity;
    size_t writeIndex = 0;
    size_t readIndex = 0;
    size_t count = 0;
    std::mutex mtx;
    std::condition_variable cv;

public:
    CircularBuffer(size_t capacity) : capacity(capacity)
    {
        buffer.resize(capacity, std::vector<short>(BUFFER_SIZE));
    }

    void push(const std::vector<short> &data)
    {
        std::unique_lock<std::mutex> lock(mtx);
        buffer[writeIndex] = data;
        writeIndex = (writeIndex + 1) % capacity;
        if (count < capacity)
            ++count;
        else
            readIndex = (readIndex + 1) % capacity; // Перезапись старых данных
        cv.notify_one();
    }

    bool pop(std::vector<short> &data)
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]
                { return count > 0 || stopFlag; });
        if (count == 0)
            return false;

        data = buffer[readIndex];
        readIndex = (readIndex + 1) % capacity;
        --count;
        return true;
    }
};

void recordAudio(CircularBuffer &circBuffer)
{
    HWAVEIN hWaveIn;
    WAVEFORMATEX waveFormat;
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = 1;
    waveFormat.nSamplesPerSec = SAMPLE_RATE;
    waveFormat.nAvgBytesPerSec = SAMPLE_RATE * sizeof(short);
    waveFormat.nBlockAlign = sizeof(short);
    waveFormat.wBitsPerSample = 16;
    waveFormat.cbSize = 0;

    if (waveInOpen(&hWaveIn, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
    {
        std::cerr << "Error opening recording device" << std::endl;
        return;
    }

    WAVEHDR waveHdr[NUM_BUFFERS];
    std::vector<short> buffers[NUM_BUFFERS];

    for (int i = 0; i < NUM_BUFFERS; ++i)
    {
        buffers[i].resize(BUFFER_SIZE);
        waveHdr[i].lpData = reinterpret_cast<LPSTR>(buffers[i].data());
        waveHdr[i].dwBufferLength = BUFFER_SIZE * sizeof(short);
        waveHdr[i].dwFlags = 0;
        waveHdr[i].dwLoops = 1;

        waveInPrepareHeader(hWaveIn, &waveHdr[i], sizeof(WAVEHDR));
        waveInAddBuffer(hWaveIn, &waveHdr[i], sizeof(WAVEHDR));
    }

    waveInStart(hWaveIn);

    while (!stopFlag)
    {
        for (int i = 0; i < NUM_BUFFERS; ++i)
        {
            if (waveHdr[i].dwFlags & WHDR_DONE)
            {
                circBuffer.push(buffers[i]); // Добавляем данные в кольцевой буфер

                waveInUnprepareHeader(hWaveIn, &waveHdr[i], sizeof(WAVEHDR));
                waveInPrepareHeader(hWaveIn, &waveHdr[i], sizeof(WAVEHDR));
                waveInAddBuffer(hWaveIn, &waveHdr[i], sizeof(WAVEHDR));
            }
        }
    }

    waveInStop(hWaveIn);
    waveInClose(hWaveIn);
}

void playbackAudio(CircularBuffer &circBuffer)
{
    HWAVEOUT hWaveOut;
    WAVEFORMATEX waveFormat;
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = 1;
    waveFormat.nSamplesPerSec = SAMPLE_RATE;
    waveFormat.nAvgBytesPerSec = SAMPLE_RATE * sizeof(short);
    waveFormat.nBlockAlign = sizeof(short);
    waveFormat.wBitsPerSample = 16;
    waveFormat.cbSize = 0;

    if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
    {
        std::cerr << "Error opening playback device" << std::endl;
        return;
    }

    WAVEHDR waveHdr;
    std::vector<short> buffer;

    // Ждем 5 секунд перед началом воспроизведения
    std::this_thread::sleep_for(std::chrono::seconds(DELAY_SECONDS));

    while (!stopFlag)
    {
        if (circBuffer.pop(buffer)) // Извлекаем данные из кольцевого буфера
        {
            waveHdr.lpData = reinterpret_cast<LPSTR>(buffer.data());
            waveHdr.dwBufferLength = BUFFER_SIZE * sizeof(short);
            waveHdr.dwFlags = 0;
            waveHdr.dwLoops = 0;

            waveOutPrepareHeader(hWaveOut, &waveHdr, sizeof(WAVEHDR));
            waveOutWrite(hWaveOut, &waveHdr, sizeof(WAVEHDR));

            while (!(waveHdr.dwFlags & WHDR_DONE) && !stopFlag)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            waveOutUnprepareHeader(hWaveOut, &waveHdr, sizeof(WAVEHDR));
        }
    }

    waveOutClose(hWaveOut);
}

int main()
{
    CircularBuffer circBuffer(DELAY_SECONDS * SAMPLE_RATE / BUFFER_SIZE);

    std::thread recordThread(recordAudio, std::ref(circBuffer));
    std::thread playbackThread(playbackAudio, std::ref(circBuffer));

    std::cout << "Нажмите Enter для завершения..." << std::endl;
    std::cin.get();

    stopFlag = true;

    recordThread.join();
    playbackThread.join();

    std::cout << "Программа завершена." << std::endl;
    return 0;
}
