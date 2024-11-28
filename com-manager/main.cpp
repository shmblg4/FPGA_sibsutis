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
#define BUFFER_SIZE 8192
#define NUM_BUFFERS 2
#define DELAY_SECONDS 2

std::atomic<bool> stopFlag(false);

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
            readIndex = (readIndex + 1) % capacity; // перепись старых данных
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

void findComPorts()
{
    char portName[10];
    for (int i = 256; i >= 1; --i)
    {
        snprintf(portName, sizeof(portName), "COM%d", i);

        HANDLE hCom = CreateFileA(portName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);

        if (hCom != INVALID_HANDLE_VALUE)
        {
            std::cout << portName << " доступен." << std::endl;
            CloseHandle(hCom);
        }
    }
}

HANDLE openCOMPort(const std::string &portName)
{
    HANDLE hCom = CreateFileA(
        portName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);

    if (hCom == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Не удалось открыть COM-порт: " << GetLastError() << std::endl;
        return NULL;
    }

    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(hCom, &dcb))
    {
        std::cerr << "Не удалось получить параметры COM-порта" << std::endl;
        CloseHandle(hCom);
        return NULL;
    }

    dcb.BaudRate = 921600;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;

    if (!SetCommState(hCom, &dcb))
    {
        std::cerr << "Не удалось установить параметры COM-порта" << std::endl;
        CloseHandle(hCom);
        return NULL;
    }

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;

    if (!SetCommTimeouts(hCom, &timeouts))
    {
        std::cerr << "Не удалось установить таймауты для COM-порта" << std::endl;
        CloseHandle(hCom);
        return NULL;
    }

    return hCom;
}

void playbackAudio(CircularBuffer &buffer)
{
    HWAVEOUT hWaveOut;
    WAVEFORMATEX waveFormat;
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = 1; // моно
    waveFormat.nSamplesPerSec = SAMPLE_RATE;
    waveFormat.wBitsPerSample = 16;
    waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
    waveFormat.cbSize = 0;

    if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
    {
        std::cerr << "Не удалось открыть вывод звука." << std::endl;
        return;
    }

    WAVEHDR waveHeader;
    std::vector<short> audioData(BUFFER_SIZE);

    while (!stopFlag)
    {
        if (buffer.pop(audioData))
        {
            waveHeader.lpData = reinterpret_cast<LPSTR>(audioData.data());
            waveHeader.dwBufferLength = audioData.size() * sizeof(short);
            waveHeader.dwFlags = 0;

            if (waveOutPrepareHeader(hWaveOut, &waveHeader, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
            {
                std::cerr << "Не удалось подготовить заголовок для вывода." << std::endl;
                waveOutClose(hWaveOut);
                return;
            }

            waveOutWrite(hWaveOut, &waveHeader, sizeof(WAVEHDR));
        }
    }

    waveOutClose(hWaveOut);
}

void receiveFromCOMPort(HANDLE hCom, CircularBuffer &circBuffer)
{
    std::vector<short> buffer(BUFFER_SIZE);

    while (!stopFlag)
    {
        DWORD bytesRead = 0;
        if (ReadFile(hCom, buffer.data(), buffer.size() * sizeof(short), &bytesRead, NULL))
        {
            if (bytesRead > 0)
            {
                buffer.resize(bytesRead / sizeof(short));
                circBuffer.push(buffer);
            }
        }
        else
        {
            std::cerr << "Ошибка чтения из COM-порта: " << GetLastError() << std::endl;
        }
    }
}

void recordAudio(CircularBuffer &buffer)
{
    HWAVEIN hWaveIn;
    WAVEFORMATEX waveFormat;
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = 1; // моно
    waveFormat.nSamplesPerSec = SAMPLE_RATE;
    waveFormat.wBitsPerSample = 16;
    waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
    waveFormat.cbSize = 0;

    if (waveInOpen(&hWaveIn, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
    {
        std::cerr << "Не удалось открыть запись звука." << std::endl;
        return;
    }

    WAVEHDR waveHeader;
    std::vector<short> audioData(BUFFER_SIZE);
    waveHeader.lpData = reinterpret_cast<LPSTR>(audioData.data());
    waveHeader.dwBufferLength = BUFFER_SIZE * sizeof(short);
    waveHeader.dwFlags = 0;

    if (waveInPrepareHeader(hWaveIn, &waveHeader, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
    {
        std::cerr << "Не удалось подготовить заголовок для записи." << std::endl;
        waveInClose(hWaveIn);
        return;
    }

    waveInStart(hWaveIn);

    while (!stopFlag)
    {
        if (waveInAddBuffer(hWaveIn, &waveHeader, sizeof(WAVEHDR)) == MMSYSERR_NOERROR)
        {
            buffer.push(audioData);
        }
    }

    waveInStop(hWaveIn);
    waveInClose(hWaveIn);
}

void sendToCOMPort(HANDLE hCom, CircularBuffer &circBuffer)
{
    std::vector<short> buffer;

    while (!stopFlag)
    {
        if (circBuffer.pop(buffer))
        {
            DWORD bytesWritten = 0;
            if (!WriteFile(hCom, buffer.data(), buffer.size() * sizeof(short), &bytesWritten, NULL))
            {
                std::cerr << "Ошибка записи в COM-порт: " << GetLastError() << std::endl;
            }
        }
    }
}

int main()
{
    findComPorts();
    std::string Port;
    std::cout << "Выберите COM порт (например, COM1): ";
    std::cin >> Port;

    HANDLE hComWrite = openCOMPort(Port);
    if (!hComWrite)
        return 1;

    HANDLE hComRead = openCOMPort(Port);
    if (!hComRead)
    {
        CloseHandle(hComWrite);
        return 1;
    }

    CircularBuffer recordBuffer(DELAY_SECONDS * SAMPLE_RATE / BUFFER_SIZE);
    CircularBuffer playbackBuffer(DELAY_SECONDS * SAMPLE_RATE / BUFFER_SIZE);

    std::thread recordThread(recordAudio, std::ref(recordBuffer));
    std::thread sendThread(sendToCOMPort, hComWrite, std::ref(recordBuffer));
    std::thread receiveThread(receiveFromCOMPort, hComRead, std::ref(playbackBuffer));
    std::thread playbackThread(playbackAudio, std::ref(playbackBuffer));

    std::cin.get();
    std::cin.get();

    stopFlag = true;

    recordThread.join();
    sendThread.join();
    receiveThread.join();
    playbackThread.join();

    CloseHandle(hComWrite);
    CloseHandle(hComRead);

    return 0;
}
