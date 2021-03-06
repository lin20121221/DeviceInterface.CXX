#ifndef _LABNATION_INTERFACESERVER_H
#define _LABNATION_INTERFACESERVER_H

#include "smartscope.h"
#include "smartscopeusb.h"

#ifdef DNSSD
#include <dns_sd.h>
#else
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/thread-watch.h>
#include <avahi-common/simple-watch.h>
#include <avahi-client/publish.h>
#endif

#include <pthread.h>
#include <functional>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#define VERSION_MAJOR  1
#define VERSION_MINOR  6
#define VERSION ((VERSION_MAJOR << 8) + VERSION_MINOR)

#if DEBUG
#define FLAVOR_DEBUG "-debug"
#else
#define FLAVOR_DEBUG ""
#endif

#ifndef BUILD_VERSION
#define BUILD_VERSION "sometime"
#endif

#if LEDE
#define FLAVOR "lede" FLAVOR_DEBUG
#elif DNSSD
#define FLAVOR "dnssd" FLAVOR_DEBUG
#else
#define FLAVOR "vanilla" FLAVOR_DEBUG
#endif

#define TIMEOUT_DATA 4
#define TIMEOUT_CTRL 4

namespace labnation {

class NetException: public std::exception {
private:
    std::string _message;
public:
    explicit NetException(const char* message, ...);
    virtual const char* what() const throw() {
        return _message.c_str();
    }
};

class InterfaceServer {

enum Command : uint8_t {
  SERIAL          = 0x0d,
  FLUSH           = 0x0e,
  DISCONNECT      = 0x0f,
  GET             = 0x18,
  SET             = 0x19,
  DATA            = 0x1a,
  PIC_FW_VERSION  = 0x1b,
  FLASH_FPGA      = 0x24,
  DATA_PORT       = 0x2a,
  ACQUISITION     = 0x34,
#ifdef LEDE
  LEDE_LIST_APS   = 0x40,
  LEDE_RESET      = 0x41,
  LEDE_CONNECT_AP = 0x42,
  LEDE_REBOOT     = 0x43,
  LEDE_MODE_AP    = 0x44,
#endif
  SERVER_VERSION  = 0x50,
  SERVER_INFO     = 0x51,
};

struct __attribute__ ((__packed__)) Message {
  uint32_t length;
  Command cmd;
  uint8_t data[];
};

struct __attribute__ ((__packed__)) ControllerMessage {
  labnation::SmartScopeUsb::Controller ctrl;
  uint16_t addr;
  uint16_t len;
  uint8_t data[];
};

public:
  InterfaceServer(SmartScopeUsb* scope);
  ~InterfaceServer();
  enum State {
    Uninitialized = 0,
    Stopped = 1,
    Stopping = 2,
    Started = 3,
    Starting = 4,
    Destroying = 5,
    Destroyed = 6
  };
  State GetState();
  void SetState(State state);
  void Start();
  void Stop();
  void Destroy();

  void ManageState();
  void DataSocketServer();
  void ControlSocketServer();

private:
  static const int ACQUISITION_PACKET_SIZE = SZ_HDR + FETCH_SIZE_MAX;
  static const int DATA_SOCKET_BUFFER_SIZE = ACQUISITION_PACKET_SIZE * 2;
  static const int HDR_SZ = 4;
  static const int DATA_BUF_SIZE = 8 * 1024;
  static const int BUF_SIZE = 128 * 1024;
  static const int MSG_BUF_SIZE = 1024 * 1024;

  uint8_t *ss_buf = NULL;
  uint8_t *tx_buf = NULL;
  uint8_t *msg_buf = NULL;

  std::function<void(InterfaceServer*)> _stateChanged = NULL;
  const char* SERVICE_TYPE = "_sss._tcp";
  const char* REPLY_DOMAIN = "local.";
  const char* TXT_DATA_PORT = "DATA_PORT";

  State _stateRequested = Uninitialized;
  State _state = Uninitialized;
  labnation::SmartScopeUsb* _scope = NULL;
  uint16_t _port = 0;
  uint16_t _port_data = 0;
  bool _connected = false;
#ifdef LEDE
  bool _changing_ap = false;
#endif
  bool _disconnect_called = false;

  /* Zeroconf service registration */

#ifdef DNSSD
  DNSServiceRef _dnsService = NULL;
  static void ServiceRegistered(
      DNSServiceRef sdRef,
      DNSServiceFlags flags,
      DNSServiceErrorType errorCode,
      const char *name,
      const char *regtype,
      const char *domain,
      void       *context
  );
#else
  AvahiThreadedPoll* _avahi_poll = NULL;
  AvahiClient* _avahi_client = NULL;
  AvahiEntryGroup* _avahi_entry_group = NULL;
  static void AvahiCallback(AvahiClient *s, AvahiClientState state, void *data);
  static void AvahiGroupChanged(AvahiEntryGroup *g, AvahiEntryGroupState state, void* userdata);

#endif
  void RegisterService();
  void UnregisterService();

  /* Threads */

  pthread_t _thread_state = 0;
  pthread_t _thread_data = 0;
  pthread_t _thread_ctrl = 0;

  static void* ThreadStartManageState(void * ctx);
  static void* ThreadStartDataSocketServer(void * ctx);
  static void* ThreadStartControlSocketServer(void * ctx);

  void Disconnect();
  void CleanSocketThread(pthread_t *thread, int *listener_socket, int *socket);

  /* Network */
  int _sock_ctrl_listen = -1;
  int _sock_ctrl = -1;
  int _sock_data_listen = -1;
  int _sock_data = -1;

  static int StartServer(const char * port);

};

}

#endif // _LABNATION_INTERFACESERVER_H
