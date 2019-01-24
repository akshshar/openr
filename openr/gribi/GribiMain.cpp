#include "GribiRoute.h"
#include <csignal>
#include <iostream>
#include <string>

using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::CompletionQueue;
using grpc::Status;
using gribi::gRIBI;
using gribi::AFTOperation;
using gribi::ModifyRequest;
using gribi::ModifyResponse;
using namespace openr;

std::string
getEnvVar(std::string const & key)
{
    char * val = std::getenv( key.c_str() );
    return val == NULL ? std::string("") : std::string(val);
}

//GribiVrf* vrfhandler_signum;

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


    route_signum = std::move(gribi_route);

    signal(SIGINT, signalHandler);
    LOG(INFO) << "Press control-c to quit";
    
    LOG(INFO) <<  "EMILY Gribi stub creation";
    auto stub_ = gribi::gRIBI::NewStub(channel);
    ModifyRequest modify_req;
    ModifyResponse modify_res;
    ClientContext context;

    auto timeout=2;
    // Set timeout for RPC
    std::chrono::system_clock::time_point deadline =
        std::chrono::system_clock::now() + std::chrono::seconds(timeout);

    context.set_deadline(deadline);
    std::shared_ptr<ClientReaderWriter<ModifyRequest, ModifyResponse> > stream(stub_->Modify(&context));

    /*
    // ROUTE
    LOG(INFO) << "EMILY: ROUTE DELETE";
    gribi::AFTOperation* route_op = modify_req.add_operation();
    route_op->set_id(2200);
    route_op->set_network_instance("default");
    route_op->set_op(::gribi::AFTOperation_Operation::AFTOperation_Operation_DELETE);
    // Key
    auto route_op_entry_key = route_op->mutable_ipv4();
    route_op_entry_key->set_prefix("121.1.1.0/24");
    // entry
    auto route_op_entry = route_op_entry_key->mutable_ipv4_entry();
    route_op_entry->set_decapsulate_header(gribi_aft::enums::OPENCONFIGAFTENCAPSULATIONHEADERTYPE_IPV4);

    auto route_nhg = route_op_entry->mutable_next_hop_group();
    route_nhg->set_value(1);

    auto route_packets_forwarded = route_op_entry->mutable_packets_forwarded();
    route_packets_forwarded->set_value(0);

    auto route_octets_forwarded = route_op_entry->mutable_octets_forwarded();
    route_octets_forwarded->set_value(0);
    
    LOG(INFO) << "EMILY: NHG DELETE";
    // NHG
    gribi::AFTOperation* nhg_op = modify_req.add_operation();
    nhg_op->set_id(2100);
    nhg_op->set_network_instance("default");
    nhg_op->set_op(::gribi::AFTOperation_Operation::AFTOperation_Operation_DELETE);
    // key
    auto nhg_key = nhg_op->mutable_next_hop_group();
    nhg_key->set_id(1);
    // entry
    auto nhg_entry = nhg_key->mutable_next_hop_group();
    auto nhg_entry_backup_nhg = nhg_entry->mutable_backup_next_hop_group();
    nhg_entry_backup_nhg->set_value(0);
    auto nhg_entry_color = nhg_entry->mutable_color();
    nhg_entry_color->set_value(0);
    gribi_aft::Afts_NextHopGroup_NextHopKey * nhg_entry_nh_key = nhg_entry->add_next_hop();

    nhg_entry_nh_key->set_index(1);
    auto nhg_entry_nh_key_nh = nhg_entry_nh_key->mutable_next_hop();

    auto nhg_entry_nh_key_nh_weight = nhg_entry_nh_key_nh->mutable_weight();
    nhg_entry_nh_key_nh_weight->set_value(100);

    LOG(INFO) << "EMILY: NH DELETE";
    // NH
    gribi::AFTOperation* nh_op = modify_req.add_operation();
    nh_op->set_id(2000);
    nh_op->set_network_instance("default");
    nh_op->set_op(::gribi::AFTOperation_Operation::AFTOperation_Operation_DELETE);
    // key
    auto nh_key = nh_op->mutable_next_hop();
    nh_key->set_index(1);
    // entry
    auto nh_entry = nh_key->mutable_next_hop();
    nh_entry->set_encapsulate_header(gribi_aft::enums::OPENCONFIGAFTENCAPSULATIONHEADERTYPE_IPV4);

    auto nh_entry_if_ref = nh_entry->mutable_interface_ref();
    auto nh_entry_if_ref_if = nh_entry_if_ref->mutable_interface();
    nh_entry_if_ref_if->set_value("GigabitEthernet0/0/0/0");
    auto nh_entry_if_ref_subif = nh_entry_if_ref->mutable_subinterface();
    nh_entry_if_ref_subif->set_value(0);
    auto nh_entry_ip = nh_entry->mutable_ip_address();
    nh_entry_ip->set_value("10.0.0.1");
    auto nh_entry_mac = nh_entry->mutable_mac_address();
    nh_entry_mac->set_value("");
    nh_entry->set_origin_protocol(gribi_aft::enums::OPENCONFIGPOLICYTYPESINSTALLPROTOCOLTYPE_STATIC);
    */ 
    LOG(INFO) << "EMILY: NH ADD";
    // NH
    gribi::AFTOperation* nh_op = modify_req.add_operation();
    nh_op->set_id(1000);
    nh_op->set_network_instance("default");
    nh_op->set_op(::gribi::AFTOperation_Operation::AFTOperation_Operation_ADD);
    // key
    auto nh_key = nh_op->mutable_next_hop();
    nh_key->set_index(1);
    // entry
    auto nh_entry = nh_key->mutable_next_hop();
    nh_entry->set_encapsulate_header(gribi_aft::enums::OPENCONFIGAFTENCAPSULATIONHEADERTYPE_IPV4);
    
    auto nh_entry_if_ref = nh_entry->mutable_interface_ref();
    auto nh_entry_if_ref_if = nh_entry_if_ref->mutable_interface();
    nh_entry_if_ref_if->set_value("GigabitEthernet0/0/0/0");
    auto nh_entry_if_ref_subif = nh_entry_if_ref->mutable_subinterface();
    nh_entry_if_ref_subif->set_value(0);
    auto nh_entry_ip = nh_entry->mutable_ip_address();
    nh_entry_ip->set_value("10.0.0.1");
    auto nh_entry_mac = nh_entry->mutable_mac_address();
    nh_entry_mac->set_value("");
    nh_entry->set_origin_protocol(gribi_aft::enums::OPENCONFIGPOLICYTYPESINSTALLPROTOCOLTYPE_STATIC);
    
    LOG(INFO) << "EMILY: NHG ADD";
    // NHG
    gribi::AFTOperation* nhg_op = modify_req.add_operation();
    nhg_op->set_id(1100);
    nhg_op->set_network_instance("default");
    nhg_op->set_op(::gribi::AFTOperation_Operation::AFTOperation_Operation_ADD);
    // key
    auto nhg_key = nhg_op->mutable_next_hop_group();
    nhg_key->set_id(1);
    // entry
    auto nhg_entry = nhg_key->mutable_next_hop_group();
    auto nhg_entry_backup_nhg = nhg_entry->mutable_backup_next_hop_group();
    nhg_entry_backup_nhg->set_value(0);
    auto nhg_entry_color = nhg_entry->mutable_color();
    nhg_entry_color->set_value(0);
    gribi_aft::Afts_NextHopGroup_NextHopKey * nhg_entry_nh_key = nhg_entry->add_next_hop();
    
    nhg_entry_nh_key->set_index(1);
    auto nhg_entry_nh_key_nh = nhg_entry_nh_key->mutable_next_hop();

    auto nhg_entry_nh_key_nh_weight = nhg_entry_nh_key_nh->mutable_weight();
    nhg_entry_nh_key_nh_weight->set_value(100);    
    
    // ROUTE
    LOG(INFO) << "EMILY: ROUTE ADD";
    gribi::AFTOperation* route_op = modify_req.add_operation();
    route_op->set_id(1200);
    route_op->set_network_instance("default");
    route_op->set_op(::gribi::AFTOperation_Operation::AFTOperation_Operation_ADD); 
    // Key
    auto route_op_entry_key = route_op->mutable_ipv4();
    route_op_entry_key->set_prefix("121.1.1.0/24");
    // entry
    auto route_op_entry = route_op_entry_key->mutable_ipv4_entry();
    route_op_entry->set_decapsulate_header(gribi_aft::enums::OPENCONFIGAFTENCAPSULATIONHEADERTYPE_IPV4);

    auto route_nhg = route_op_entry->mutable_next_hop_group();
    route_nhg->set_value(1);

    auto route_packets_forwarded = route_op_entry->mutable_packets_forwarded();
    route_packets_forwarded->set_value(0);

    auto route_octets_forwarded = route_op_entry->mutable_octets_forwarded();
    route_octets_forwarded->set_value(0); 
    
    LOG(INFO) << "EMILY: DONE ADDING";
    stream->Write(modify_req);
    LOG(INFO) << "EMILY: WRITE TO STREAM";

    stream->WritesDone();
    LOG(INFO) << "EMILY: STREAM WRITE DONE";
    while (stream->Read(&modify_res)) {
        LOG(INFO)  << "EMILY Gribi stub creation";
    }

    Status status = stream->Finish();
    if (status.ok()) {
        LOG(INFO) << "EMILY: Modify return ok";
    } else {
        LOG(INFO) << "EMILY Modify return failed";
        LOG(INFO) << status.error_code();
    }

    return 0;
}
