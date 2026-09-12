// Exercise socket error handling independently of Wine's ICMP delivery policy.
#ifdef _WIN32
#include <winsock2.h>
static int injected_error;
static int WSAAPI injected_send(SOCKET, const char *, int size, int) {
    WSASetLastError(injected_error);
    return injected_error ? SOCKET_ERROR : size;
}
#else
#include <sys/socket.h>
#include <cerrno>
static int injected_error;
static ssize_t injected_send(int, const void *, size_t size, int) {
    errno=injected_error;
    return injected_error ? -1 : static_cast<ssize_t>(size);
}
#endif
#define send injected_send
#include "../src/datv.cpp"
#undef send
#include <cassert>

int main() {
    UdpSocket socket;
    DatvUdpSettings settings=datv_udp_defaults();
    std::strcpy(settings.ip,"127.0.0.1");
    socket.open(settings);
    unsigned char data[1316]={0};
#ifdef _WIN32
    const int refused[]={WSAECONNREFUSED,WSAECONNRESET};
    const int fatal[]={WSAEACCES,WSAENETUNREACH,WSAEWOULDBLOCK};
#else
    const int refused[]={ECONNREFUSED};
    const int fatal[]={EACCES,ENETUNREACH,EWOULDBLOCK,ECONNRESET};
#endif
    for (int error:refused) {
        injected_error=error;
        assert(!socket.packet(data));
        injected_error=0;
        assert(socket.packet(data));
    }
    for (int error:fatal) {
        injected_error=error;
        bool failed=false;
        try { socket.packet(data); }
        catch (const std::runtime_error &e) {
            failed=true;
            assert(std::string(e.what()).find(std::to_string(error))!=std::string::npos);
        }
        assert(failed);
    }
    std::puts("UDP error injection OK: port refusals recover; other errors remain fatal.");
}
