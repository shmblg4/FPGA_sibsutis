#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <xserial.hpp>

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
            readIndex = (readIndex + 1) % capacity; // overwriting old data
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

// Using xserial to work with COM ports
xserial::ComPort openCOMPort(int portNumber, unsigned long baudRate)
{
    xserial::ComPort comPort;
    while (!comPort.open(portNumber, baudRate))
    {
        Sleep(1000);
        std::cout << "Trying to open COM" << portNumber << std::endl;
    }
    std::cout << "Opened COM port COM" << portNumber << std::endl;
    return comPort;
}

void playbackAudio(CircularBuffer &buffer)
{
    HWAVEOUT hWaveOut;
    WAVEFORMATEX waveFormat;
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = 1; // mono
    waveFormat.nSamplesPerSec = SAMPLE_RATE;
    waveFormat.wBitsPerSample = 16;
    waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
    waveFormat.cbSize = 0;

    if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
    {
        std::cerr << "Failed to open audio output." << std::endl;
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
                std::cerr << "Failed to prepare header for output." << std::endl;
                waveOutClose(hWaveOut);
                return;
            }

            waveOutWrite(hWaveOut, &waveHeader, sizeof(WAVEHDR));
        }
    }

    waveOutClose(hWaveOut);
}

void receiveFromCOMPort(xserial::ComPort &comPort, CircularBuffer &circBuffer)
{
    std::vector<short> buffer(BUFFER_SIZE);

    while (!stopFlag)
    {
        unsigned long bytesRead = comPort.read(reinterpret_cast<char *>(buffer.data()), buffer.size() * sizeof(short));
        if (bytesRead > 0)
        {
            buffer.resize(bytesRead / sizeof(short));
            circBuffer.push(buffer);
        }
    }
}

void recordAudio(CircularBuffer &buffer)
{
    HWAVEIN hWaveIn;
    WAVEFORMATEX waveFormat;
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = 1; // mono
    waveFormat.nSamplesPerSec = SAMPLE_RATE;
    waveFormat.wBitsPerSample = 16;
    waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
    waveFormat.cbSize = 0;

    if (waveInOpen(&hWaveIn, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
    {
        std::cerr << "Failed to open audio recording." << std::endl;
        return;
    }

    WAVEHDR waveHeader;
    std::vector<short> audioData(BUFFER_SIZE);
    waveHeader.lpData = reinterpret_cast<LPSTR>(audioData.data());
    waveHeader.dwBufferLength = BUFFER_SIZE * sizeof(short);
    waveHeader.dwFlags = 0;

    if (waveInPrepareHeader(hWaveIn, &waveHeader, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
    {
        std::cerr << "Failed to prepare header for recording." << std::endl;
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

void sendToCOMPort(xserial::ComPort &comPort, CircularBuffer &circBuffer)
{
    std::vector<short> buffer;

    while (!stopFlag)
    {
        if (circBuffer.pop(buffer))
        {
            comPort.write(reinterpret_cast<char *>(buffer.data()), buffer.size() * sizeof(short));
        }
    }
}

int main()
{
    int Port;
    unsigned long baudRate = 921600; // Setting the required speed
    std::vector<std::string> ports;
    xserial::ComPort ListComs;
    ListComs.getListSerialPorts(std::ref(ports));
    for (auto elem : ports)
        std::cout << elem << std::endl;
    std::cout << "Select COM port (e.g., 1 for COM1): ";
    std::cin >> Port;
    // Open the COM port via xserial
    xserial::ComPort comWrite = openCOMPort(Port, baudRate);
    if (!comWrite.getStateComPort())
    {
        return 1;
    }

    xserial::ComPort comRead = openCOMPort(Port, baudRate);
    if (!comRead.getStateComPort())
    {
        comWrite.close();
        return 1;
    }

    CircularBuffer recordBuffer(DELAY_SECONDS * SAMPLE_RATE / BUFFER_SIZE);
    CircularBuffer playbackBuffer(DELAY_SECONDS * SAMPLE_RATE / BUFFER_SIZE);

    std::thread recordThread(recordAudio, std::ref(recordBuffer));
    std::thread sendThread(sendToCOMPort, std::ref(comWrite), std::ref(recordBuffer));
    std::thread receiveThread(receiveFromCOMPort, std::ref(comRead), std::ref(playbackBuffer));
    std::thread playbackThread(playbackAudio, std::ref(playbackBuffer));

    std::cin.get();
    std::cin.get();

    stopFlag = true;

    recordThread.join();
    sendThread.join();
    receiveThread.join();
    playbackThread.join();

    comWrite.close();
    comRead.close();

    return 0;
}
