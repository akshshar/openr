#include "GribiRoute.h"
#include <google/protobuf/text_format.h>

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

namespace openr {

GribiRoute::GribiRoute(std::shared_ptr<grpc::Channel> Channel)
    : channel(Channel) {}

// Gribi operation id
int op_id = 1;
// Gribi NH key index
int nh_key_idx = 1;
// Gribi NHG key index
int nhg_key_idx = 1;

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

uint32_t
GribiRoute::ipv4ToLong(const char* address)
{
    struct sockaddr_in sa;
    if (inet_pton(AF_INET, address, &(sa.sin_addr)) != 1) {
        LOG(ERROR) << "Invalid IPv4 address " << address;
        return 0;
    }

    return ntohl(sa.sin_addr.s_addr);
}

std::string
GribiRoute::longToIpv4(uint32_t nlprefix)
{
    struct sockaddr_in sa;
    char str[INET_ADDRSTRLEN];

    // Convert to hostbyte first form
    uint32_t hlprefix = htonl(nlprefix);

    sa.sin_addr.s_addr = hlprefix;

    if (inet_ntop(AF_INET,  &(sa.sin_addr), str, INET_ADDRSTRLEN)) {
        return std::string(str);
    } else {
        LOG(ERROR) << "inet_ntop conversion error: "<< strerror(errno);
        return std::string("");
    }
}


std::string
GribiRoute::ipv6ToByteArrayString(const char* address)
{
    struct in6_addr ipv6data;
    if (inet_pton(AF_INET6, address, &ipv6data) != 1 ) {
        LOG(ERROR) << "Invalid IPv6 address " << address;
        return std::string("");
    }

    const char *ptr(reinterpret_cast<const char*>(&ipv6data.s6_addr));
    std::string ipv6_charstr(ptr, ptr+16);
    return ipv6_charstr;
}


std::string
GribiRoute::ByteArrayStringtoIpv6(std::string ipv6ByteArray)
{

    struct in6_addr ipv6data;
    char str[INET6_ADDRSTRLEN];

    std::copy(ipv6ByteArray.begin(), ipv6ByteArray.end(),ipv6data.s6_addr);


    if (inet_ntop(AF_INET6,  &(ipv6data), str, INET6_ADDRSTRLEN)) {
        return std::string(str);
    } else {
        LOG(ERROR) << "inet_ntop conversion error: "<< strerror(errno);
        return std::string("");
    }
}


// V4 methods

bool
GribiRoute::addPrefixPathV4(std::string prefix,
                            uint8_t prefixLen,
                            std::string nextHopAddress,
                            std::string nextHopIf)
{
    std::unique_ptr<GribiRoute> gribi_route;
    gribi_route = std::make_unique<GribiRoute>(channel);

    route_signum = std::move(gribi_route);

    signal(SIGINT, signalHandler);
    LOG(INFO) << "Press control-c to quit";

    LOG(INFO) << "EMILY: PREFIX INFO" << prefix << "length: " << prefixLen << "nh: " << nextHopAddress << "nhif :" << nextHopIf;
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
    
    /*
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
    */
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
    //nextHopGroupOp();
    //routeOp();
    return 0;


}

void
GribiRoute::nextHopOp(std::string prefix,
                   uint8_t prefixLen,
                   std::string nextHopAddress,
                   std::string nextHopIf,
                   gribi::ModifyRequest modify_req)
{
    LOG(INFO) << "EMILY: NH ADD START";
    LOG(INFO) << "Operation ID";
    LOG(INFO) << op_id;
    op_id++;
    LOG(INFO) << "Key Idx";
    LOG(INFO) << nh_key_idx;
    nh_key_idx++;
    LOG(INFO) << "EMILY: NH ADD END";
}

void
GribiRoute::nextHopGroupOp()
{

    LOG(INFO) << "EMILY: NH ADD START";
    LOG(INFO) << "Operation ID";
    LOG(INFO) << op_id;
    op_id++;
    LOG(INFO) << "Key Idx";
    LOG(INFO) << nhg_key_idx;
    nhg_key_idx++;
    LOG(INFO) << "EMILY: NH ADD END";
}

void
GribiRoute::routeOp()
{
    LOG(INFO) << "EMILY: ROUTE ADD START";
    LOG(INFO) << "Operation ID";
    LOG(INFO) << op_id;
    op_id++;
    LOG(INFO) << "EMILY: ROUTE ADD END";
}

void
GribiRoute::labelOp()
{
    LOG(INFO) << "EMILY: LABELS NOT SUPPORTED YET";
}

void
GribiRoute::setVrfV4(std::string vrfName)
{
    LOG(INFO) << "v4 vrfname is " << vrfName;
    gribi_v4_op.set_network_instance(vrfName);
}

// V6 methods

void
GribiRoute::setVrfV6(std::string vrfName)
{
    LOG(INFO) << "v6 vrfname is " << vrfName;
    gribi_v6_op.set_network_instance(vrfName);
}
}
