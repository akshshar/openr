#include "GribiRoute.h"
#include <csignal>

using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::CompletionQueue;
using grpc::Status;
//using gribi::SLGlobal;
using namespace openr;

std::string
getEnvVar(std::string const & key)
{
    char * val = std::getenv( key.c_str() );
    return val == NULL ? std::string("") : std::string(val);
}

//GribiVrf* vrfhandler_signum;
//AsyncNotifChannel* asynchandler_signum;

std::unique_ptr<GribiRoute> route_signum;
bool sighandle_initiated = false;

void
signalHandler(int signum)
{

   if (!sighandle_initiated) {
       sighandle_initiated = true;
       LOG(INFO) << "Interrupt signal (" << signum << ") received.";

       // Clear out the last vrfRegMsg batch
       //vrfhandler_signum->vrf_msg.clear_vrfregmsgs();

       // Create a fresh SLVrfRegMsg batch for cleanup
       //vrfhandler_signum->vrfRegMsgAdd("default");

       //vrfhandler_signum->unregisterVrf(AF_INET);
       //vrfhandler_signum->unregisterVrf(AF_INET6);

       // Shutdown the Async Notification Channel
       //asynchandler_signum->Shutdown();

       //terminate program
       exit(signum);
    }
}

#if 0
void routeplay(std::unique_ptr<GribiRoute> & gribi_route) {

    //gribi_route->setVrfV4("default");
    // Insert routes - prefix, prefixlen, admindistance, nexthopaddress, nexthopif one by one
    gribi_route->insertAddBatchV4("20.0.1.0", 24, 120, "14.1.1.10","GigabitEthernet0/0/0/0");
    gribi_route->insertAddBatchV4("20.0.1.0", 24, 120, "15.1.1.10","GigabitEthernet0/0/0/1");
    gribi_route->insertAddBatchV4("23.0.1.0", 24, 120, "14.1.1.10","GigabitEthernet0/0/0/0");
    gribi_route->insertAddBatchV4("23.0.1.0", 24, 120, "15.1.1.10","GigabitEthernet0/0/0/1");
    gribi_route->insertAddBatchV4("30.0.1.0", 24, 120, "14.1.1.10","GigabitEthernet0/0/0/0");

    // Push route batch into the IOS-XR RIB
    gribi_route->routev4Op(gribi::SL_OBJOP_UPDATE);

    gribi::SLRoutev4 routev4;
    bool response = gribi_route->getPrefixPathsV4(routev4,"default", "23.0.1.0", 24);

    LOG(INFO) << "Prefix " << gribi_route->longToIpv4(routev4.prefix());
    for(int path_cnt=0; path_cnt < routev4.pathlist_size(); path_cnt++) {
        LOG(INFO) << "NextHop Interface: "
                  << routev4.pathlist(path_cnt).nexthopinterface().name();

        LOG(INFO) << "NextHop Address "
                  << gribi_route->longToIpv4(routev4.pathlist(path_cnt).nexthopaddress().v4address());
    }


    gribi_route->addPrefixPathV4("30.0.1.0", 24, "15.1.1.10", "GigabitEthernet0/0/0/1");
    gribi_route->addPrefixPathV4("30.0.1.0", 24, "16.1.1.10", "GigabitEthernet0/0/0/2");

    gribi_route->deletePrefixPathV4("30.0.1.0", 24,"15.1.1.10", "GigabitEthernet0/0/0/1");

    gribi_route->setVrfV6("default");
    // Create a v6 route batch, same principle as v4
    gribi_route->insertAddBatchV6("2002:aa::0", 64, 120, "2002:ae::3", "GigabitEthernet0/0/0/0");
    gribi_route->insertAddBatchV6("2003:aa::0", 64, 120, "2002:ae::4", "GigabitEthernet0/0/0/1");

    gribi_route->insertAddBatchV6("face:b00c::", 64, 120, "fe80::a00:27ff:feb5:793c", "GigabitEthernet0/0/0/1");

    // Push route batch into the IOS-XR RIB
    gribi_route->routev6Op(gribi::SL_OBJOP_ADD);

    gribi::SLRoutev6 routev6;
    response = gribi_route->getPrefixPathsV6(routev6,"default", "2002:aa::0", 64);

    LOG(INFO) << "Prefix " << gribi_route->ByteArrayStringtoIpv6(routev6.prefix());
    int path_cnt=0;
    for(int path_cnt=0; path_cnt < routev6.pathlist_size(); path_cnt++) {
        LOG(INFO) << "NextHop Interface: "
                  << routev6.pathlist(path_cnt).nexthopinterface().name();

        LOG(INFO) << "NextHop Address "
                  << gribi_route->ByteArrayStringtoIpv6(routev6.pathlist(path_cnt).nexthopaddress().v6address());
    }

    // Let's create a delete route batch for v4
    //gribi_route->insertDeleteBatchV4("20.0.1.0", 24);
    //gribi_route->insertDeleteBatchV4("23.0.1.0", 24);

    // Push route batch into the IOS-XR RIB
    //gribi_route->routev4Op(gribi::SL_OBJOP_DELETE);

}
#endif

int main(int argc, char** argv) {

    std::string ifname = "Hg0_0_1_0";

    enum IfNameTypes
    {
        GIG,
        TEN_GIG,
        FORTY_GIG,
        TWENTY_FIVE_GIG,
        HUNDRED_GIG,
        MGMT
    };

    std::map<std::string, IfNameTypes>
    iosxrLnxIfname = {{"Gi",GIG}, {"Tg", TEN_GIG},
                      {"Fg",FORTY_GIG}, {"Tf", TWENTY_FIVE_GIG},
                      {"Hg",HUNDRED_GIG}, {"Mg", MGMT}};


    auto ifnamePrefix = "";

    switch (iosxrLnxIfname[ifname.substr(0,2)]) {
    case GIG:
        ifnamePrefix = "GigabitEthernet";
        break;
    case TEN_GIG:
        ifnamePrefix = "TenGigE";
        break;
    case FORTY_GIG:
        ifnamePrefix = "FortyGigE";
        break;
    case TWENTY_FIVE_GIG:
        ifnamePrefix = "TwentyFiveGigE";
        break;
    case HUNDRED_GIG:
        ifnamePrefix = "HundredGigE";
        break;
    case MGMT:
        ifnamePrefix = "MgmtEth";
        break;
    default:
        LOG(ERROR) << "Invalid Interface " << ifname;
    }

    std::replace(ifname.begin(),
                 ifname.end(),
                 '_','/');

    auto result = ifnamePrefix + ifname.substr(2);

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

    std::unique_ptr<GribiRoute> gribi_route;

    gribi_route = std::make_unique<GribiRoute>(channel);
    //routeplay(gribi_route);

    //asynchandler_signum = &asynchandler;
    //vrfhandler_signum = &vrfhandler;
    route_signum = std::move(gribi_route);

    signal(SIGINT, signalHandler);
    LOG(INFO) << "Press control-c to quit";
    thread_.join();

    return 0;
}
