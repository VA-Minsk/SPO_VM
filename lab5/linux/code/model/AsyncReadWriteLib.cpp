//
// Created by alex on 02.04.17.
//

#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <aio.h>
#include <cstring>

#include "AsyncReadWriteLib.h"

#include "../exceptions/AllExceptions.h"

std::string asyncReadAllFile(const std::string &fileWay)
{
    using namespace VA_const;

    auto fsize = filelength(fileWay.c_str());
    if (fsize < 0)
    {
        throw FileLengthException();
    }

    char *buff = new char[fsize+10];

    auto fileDescriptor = open(fileWay.c_str(), O_CREAT | O_RDWR | O_EXCL, S_IRUSR | S_IWUSR);

    aiocb controlBlock = Make_aiocb(fileDescriptor, buff, fsize, RWFinishedSignal, readFinished, LIO_READ);
    aio_read(&controlBlock);

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, RWFinishedSignal);

    int sigRes = 0;

    if (sigwait(&set, &sigRes) != 0)
    {
        throw WaitSignalException();
    }

    std::string res(buff, (unsigned long) fsize);
    delete[] buff;
    close(fileDescriptor);
    return res;
}

long filelength(const char *fileName) {
    struct stat st;
    stat(fileName, &st);
    return st.st_size;
}

aiocb Make_aiocb(const int &fileDescriptor, void *bufferToRead, const long &sizeOfBuffer, int signal, int sigval_int,
                 int operationCode) {
    sigevent tmpSigevent;
    tmpSigevent.sigev_notify = SIGEV_SIGNAL;
    tmpSigevent.sigev_signo = signal;
    tmpSigevent.sigev_value.sival_int = sigval_int;

    aiocb controlBlock;
    memset(&controlBlock, 0, sizeof(struct aiocb));
    controlBlock.aio_fildes = fileDescriptor;           //file descriptor
    controlBlock.aio_offset = 0;                        //read from begin
    controlBlock.aio_buf = bufferToRead;                //Location of buffer
    controlBlock.aio_nbytes = (size_t) sizeOfBuffer;    //Length of transfer
    controlBlock.aio_reqprio = 0;                       //Request priority
    controlBlock.aio_sigevent = tmpSigevent;            //Notification method
    controlBlock.aio_lio_opcode = operationCode;        //Operation to be performed

    return controlBlock;
}
