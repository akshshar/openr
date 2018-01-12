#include "ServiceLayerRoute.h"
#include <csignal>

using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::CompletionQueue;
using grpc::Status;
using service_layer::SLInitMsg;
using service_layer::SLVersion;
using service_layer::SLGlobal;
using namespace openr;

std::string 
getEnvVar(std::string const & key)
{
    char * val = std::getenv( key.c_str() );
    return val == NULL ? std::string("") : std::string(val);
}


IosxrslVrf* vrfhandler_signum;
std::unique_ptr<IosxrslRshuttle> rshuttle_signum;
AsyncNotifChannel* asynchandler_signum;
bool sighandle_initiated = false;

void 
signalHandler(int signum)
{

   if (!sighandle_initiated) {
       sighandle_initiated = true;
       VLOG(1) << "Interrupt signal (" << signum << ") received.";

       // Clear out the last vrfRegMsg batch
       vrfhandler_signum->vrf_msg.clear_vrfregmsgs();

       // Create a fresh SLVrfRegMsg batch for cleanup
       vrfhandler_signum->vrfRegMsgAdd("default");

       vrfhandler_signum->unregisterVrf(AF_INET);
       vrfhandler_signum->unregisterVrf(AF_INET6);

       // Shutdown the Async Notification Channel  
       asynchandler_signum->Shutdown();

       //terminate program  
       exit(signum);  
    } 
}


void routeplay(std::unique_ptr<IosxrslRshuttle> & iosxrsl_rshuttle) {

    iosxrsl_rshuttle->setVrfV4("default");
    // Insert routes - prefix, prefixlen, admindistance, nexthopaddress, nexthopif one by one
    iosxrsl_rshuttle->insertAddBatchV4("20.0.1.0", 24, 120, "14.1.1.10","GigabitEthernet0/0/0/0");
    iosxrsl_rshuttle->insertAddBatchV4("20.0.1.0", 24, 120, "15.1.1.10","GigabitEthernet0/0/0/1");
    iosxrsl_rshuttle->insertAddBatchV4("23.0.1.0", 24, 120, "14.1.1.10","GigabitEthernet0/0/0/0");
    iosxrsl_rshuttle->insertAddBatchV4("23.0.1.0", 24, 120, "15.1.1.10","GigabitEthernet0/0/0/1");
    iosxrsl_rshuttle->insertAddBatchV4("30.0.1.0", 24, 120, "14.1.1.10","GigabitEthernet0/0/0/0");

    // Push route batch into the IOS-XR RIB
    iosxrsl_rshuttle->routev4Op(service_layer::SL_OBJOP_UPDATE);

    service_layer::SLRoutev4 routev4;
    bool response = iosxrsl_rshuttle->getPrefixPathsV4(routev4,"default", "23.0.1.0", 24);

    LOG(INFO) << "Prefix " << iosxrsl_rshuttle->longToIpv4(routev4.prefix());
    for(int path_cnt=0; path_cnt < routev4.pathlist_size(); path_cnt++) {
        LOG(INFO) << "NextHop Interface: "
                  << routev4.pathlist(path_cnt).nexthopinterface().name();

        LOG(INFO) << "NextHop Address "
                  << iosxrsl_rshuttle->longToIpv4(routev4.pathlist(path_cnt).nexthopaddress().v4address());
    }


    iosxrsl_rshuttle->addPrefixPathV4("30.0.1.0", 24, "15.1.1.10", "GigabitEthernet0/0/0/1");
    iosxrsl_rshuttle->addPrefixPathV4("30.0.1.0", 24, "16.1.1.10", "GigabitEthernet0/0/0/2");

    iosxrsl_rshuttle->deletePrefixPathV4("30.0.1.0", 24,"15.1.1.10", "GigabitEthernet0/0/0/1");

    iosxrsl_rshuttle->setVrfV6("default");
    // Create a v6 route batch, same principle as v4
    iosxrsl_rshuttle->insertAddBatchV6("2002:aa::0", 64, 120, "2002:ae::3", "GigabitEthernet0/0/0/0");
    iosxrsl_rshuttle->insertAddBatchV6("2003:aa::0", 64, 120, "2002:ae::4", "GigabitEthernet0/0/0/1");

    iosxrsl_rshuttle->insertAddBatchV6("face:b00c::", 64, 120, "fe80::a00:27ff:feb5:793c", "GigabitEthernet0/0/0/1");

    // Push route batch into the IOS-XR RIB
    iosxrsl_rshuttle->routev6Op(service_layer::SL_OBJOP_ADD);

    service_layer::SLRoutev6 routev6;
    response = iosxrsl_rshuttle->getPrefixPathsV6(routev6,"default", "2002:aa::0", 64);

    LOG(INFO) << "Prefix " << iosxrsl_rshuttle->ByteArrayStringtoIpv6(routev6.prefix());
    int path_cnt=0;
    for(int path_cnt=0; path_cnt < routev6.pathlist_size(); path_cnt++) {
        LOG(INFO) << "NextHop Interface: "
                  << routev6.pathlist(path_cnt).nexthopinterface().name();

        LOG(INFO) << "NextHop Address "
                  << iosxrsl_rshuttle->ByteArrayStringtoIpv6(routev6.pathlist(path_cnt).nexthopaddress().v6address());
    }


    // Let's create a delete route batch for v4 
    iosxrsl_rshuttle->insertDeleteBatchV4("20.0.1.0", 24);
    iosxrsl_rshuttle->insertDeleteBatchV4("23.0.1.0", 24);

    // Push route batch into the IOS-XR RIB
    iosxrsl_rshuttle->routev4Op(service_layer::SL_OBJOP_DELETE);

}
int main(int argc, char** argv) {


    auto server_ip = getEnvVar("SERVER_IP");
    auto server_port = getEnvVar("SERVER_PORT");

    if (server_ip == "" || server_port == "") {
        if (server_ip == "") {
            LOG(ERROR) << "SERVER_IP environment variable not set";
        }
        if (server_port == "") {
            LOG(ERROR) << "SERVER_PORT environment variable not set";
        }
        return 1;

    }


    std::string grpc_server = server_ip + ":" + server_port;
    auto channel = grpc::CreateChannel(
                              grpc_server, grpc::InsecureChannelCredentials());

 
    LOG(INFO) << "Connecting IOS-XR to gRPC server at " << grpc_server;

    AsyncNotifChannel asynchandler(channel);

    // Acquire the lock
    std::unique_lock<std::mutex> initlock(init_mutex);

    // Spawn reader thread that maintains our Notification Channel
    std::thread thread_ = std::thread(&AsyncNotifChannel::AsyncCompleteRpc, &asynchandler);


    service_layer::SLInitMsg init_msg;
    init_msg.set_majorver(service_layer::SL_MAJOR_VERSION);
    init_msg.set_minorver(service_layer::SL_MINOR_VERSION);
    init_msg.set_subver(service_layer::SL_SUB_VERSION);


    asynchandler.SendInitMsg(init_msg);

    // Wait on the mutex lock
    while (!init_success) {
        init_condVar.wait(initlock);
    }

    // Set up a new channel for vrf/route messages

    auto vrfhandler = IosxrslVrf(channel);

    // Create a new SLVrfRegMsg batch
    vrfhandler.vrfRegMsgAdd("default", 10, 500);

    // Register the SLVrfRegMsg batch for v4 and v6
    vrfhandler.registerVrf(AF_INET);
    vrfhandler.registerVrf(AF_INET6);

    std::unique_ptr<IosxrslRshuttle> iosxrsl_rshuttle;

 
    iosxrsl_rshuttle = std::make_unique<IosxrslRshuttle>(channel);
    routeplay(iosxrsl_rshuttle);

    asynchandler_signum = &asynchandler;
    vrfhandler_signum = &vrfhandler;
    rshuttle_signum = std::move(iosxrsl_rshuttle);

    signal(SIGINT, signalHandler);  
    LOG(INFO) << "Press control-c to quit";
    thread_.join();

    return 0;
}

