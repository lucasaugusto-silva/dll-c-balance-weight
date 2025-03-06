#include <windows.h>
#include <stdio.h>

// Configura a porta serial
void configure_serial_port(HANDLE hSerial, DWORD baudRate, BYTE byteSize, BYTE stopBits, BYTE parity, DWORD dtrControl) {
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(hSerial, &dcbSerialParams)) {
        fprintf(stderr, "Erro ao obter estado da porta serial\n");
        return;
    }

    dcbSerialParams.BaudRate = baudRate;
    dcbSerialParams.ByteSize = byteSize;
    dcbSerialParams.StopBits = stopBits;
    dcbSerialParams.Parity = parity;
    dcbSerialParams.fDtrControl = dtrControl;

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        fprintf(stderr, "Erro ao configurar a porta serial\n");
    }

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 2000; // Timeout total de leitura de 2 segundos
    timeouts.ReadTotalTimeoutMultiplier = 0; // Não multiplica pelo número de bytes
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(hSerial, &timeouts)) {
        fprintf(stderr, "Erro ao configurar timeouts\n");
    }
}

// Limpa o buffer da porta serial
void clear_serial_buffer(HANDLE hSerial) {
    PurgeComm(hSerial, PURGE_RXCLEAR | PURGE_TXCLEAR);
}

// Função exportada para ler o peso com timeout de 2 segundos
__declspec(dllexport) int read_weight(const char* portName, char* buffer, size_t bufferSize,
                                       DWORD baudRate, BYTE byteSize, BYTE stopBits, BYTE parity, DWORD dtrControl) {
    HANDLE hSerial = CreateFile(portName, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hSerial == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Erro ao abrir a porta serial\n");
        return -1;
    }

    configure_serial_port(hSerial, baudRate, byteSize, stopBits, parity, dtrControl);
    clear_serial_buffer(hSerial);

    // Aguarda um breve intervalo para a balança responder
    Sleep(100);

    // Configura uma estrutura para sobrepor a leitura e permitir timeout
    OVERLAPPED overlapped = {0};
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    
    if (overlapped.hEvent == NULL) {
        fprintf(stderr, "Erro ao criar evento de leitura\n");
        CloseHandle(hSerial);
        return -1;
    }

    DWORD bytesRead = 0;
    DWORD startTime = GetTickCount(); // Marca o tempo inicial
    BOOL readSuccess = FALSE;

    while (GetTickCount() - startTime < 2000) { // Loop de timeout de 2 segundos
        readSuccess = ReadFile(hSerial, buffer, bufferSize - 1, &bytesRead, &overlapped);
        
        if (readSuccess) {
            break; // Leitura bem-sucedida
        }

        // Aguarda um tempo pequeno antes de tentar novamente
        Sleep(50);
    }

    CloseHandle(overlapped.hEvent);
    
    if (!readSuccess || bytesRead == 0) {
        fprintf(stderr, "Timeout na leitura do peso\n");
        CloseHandle(hSerial);
        return 0; // Retorna "0" se não recebeu resposta
    }

    buffer[bytesRead] = '\0'; // Finaliza a string recebida

    // Captura apenas o primeiro peso válido (parando ao encontrar a sequência)
    DWORD validCharIndex = 0;
    int weightStarted = 0;
    for (DWORD i = 0; i < bytesRead; i++) {
        if ((buffer[i] >= '0' && buffer[i] <= '9') || buffer[i] == '.') {
            buffer[validCharIndex++] = buffer[i];
            weightStarted = 1; // Indicador de que começamos a capturar o peso
        } else if (weightStarted) {
            break; // Parar ao encontrar o primeiro peso completo
        }
    }
    buffer[validCharIndex] = '\0'; // Finaliza após capturar o peso

    CloseHandle(hSerial);
    return (int)validCharIndex; // Retorna o número de caracteres válidos
}
