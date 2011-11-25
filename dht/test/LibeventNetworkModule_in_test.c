#include "net/NetworkTools.h"
#include "dht/LibeventNetworkModule.c"
#include "memory/MemAllocator.h"
#include "memory/BufferAllocator.h"

struct LibeventNetworkModuleTest_context {
    struct event_base* eventBase;
    char message[50];
};

static int handleIncoming(struct DHTMessage* message, void* vcontext)
{
    struct LibeventNetworkModuleTest_context* context =
        (struct LibeventNetworkModuleTest_context*) vcontext;

    memcpy(context->message, message->bytes, message->length);
    /* A success will cause libevent to be stopped. */
    event_base_loopbreak(context->eventBase);
    return 0;
}

static int testIncoming()
{
    char buffer[32768];
    struct MemAllocator* allocator = BufferAllocator_new(buffer, 32768);

    struct LibeventNetworkModuleTest_context* context =
        calloc(sizeof(struct LibeventNetworkModuleTest_context), 1);

    const char* passThis = "Hello world, I was passed through a socket.";

    struct DHTModule* receiver = calloc(sizeof(struct DHTModule), 1);
    struct DHTModule localReceiver = {
        .name = "TestModule",
        .context = context,
        .handleIncoming = handleIncoming
    };
    memcpy(receiver, &localReceiver, sizeof(struct DHTModule));

    struct DHTModuleRegistry* reg = DHTModules_new(/*allocator*/);

    DHTModules_register(receiver, reg);

    evutil_socket_t socket = NetworkTools_bindSocket("127.0.0.1:7890");
    struct event_base* base = event_base_new();
    context->eventBase = base;

    LibeventNetworkModule_register(base, socket, 6, reg, allocator);

    evutil_socket_t socket2 = NetworkTools_bindSocket("127.0.0.1:7891");

    struct sockaddr_storage addr;
    int addrLength = sizeof(struct sockaddr_storage);
    evutil_parse_sockaddr_port("127.0.0.1:7890",
                               (struct sockaddr*)&addr,
                               &addrLength);

    sendto(socket2,
           passThis,
           strlen(passThis),
           0,
           (struct sockaddr*) &addr,
           addrLength);

    /** If the test fails, this should break eventually. */
    struct timeval twoSec = {2, 0};
    event_base_loopexit(base, &twoSec);

    event_base_dispatch(base);

    allocator->free(allocator);
    EVUTIL_CLOSESOCKET(socket);
    EVUTIL_CLOSESOCKET(socket2);

    return memcmp(context->message, passThis, strlen(passThis));
}

int main()
{
    return testIncoming();
}
