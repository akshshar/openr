#pragma once

#include <stdint.h>
#include <thread>
#include <condition_variable>
#include <memory>
#include <string>
#include <sys/socket.h>
#include <glog/logging.h>
#include <arpa/inet.h>
#include <grpc++/grpc++.h>

#include <iosxrgribi/enums.pb.h>
#include <iosxrgribi/gribi.grpc.pb.h>
#include <iosxrgribi/gribi.pb.h>
#include <iosxrgribi/gribi_aft.grpc.pb.h>
#include <iosxrgribi/gribi_aft.pb.h>


namespace openr {

class GribiRoute {

public:
    explicit GribiRoute(std::shared_ptr<grpc::Channel> Channel);

    enum PathUpdateAction
    {
        GRIBI_RSHUTTLE_PATH_ADD,
        GRIBI_RSHUTTLE_PATH_DELETE,
    };

    std::shared_ptr<grpc::Channel> channel;
    gribi::AFTOperation gribi_v4_op;
    gribi::AFTOperation gribi_v6_op;
    int op_id;
    int nh_key_idx;
    int nhg_key_idx;
    
    std::map<std::string, int> prefix_map_v4;
    std::map<std::string, int> prefix_map_v6;

    // IPv4 and IPv6 string manipulation methods

    std::string longToIpv4(uint32_t nlprefix);
    uint32_t ipv4ToLong(const char* address);
    std::string ipv6ToByteArrayString(const char* address);
    std::string ByteArrayStringtoIpv6(std::string ipv6ByteArray);

    // IPv4 methods

    void setVrfV4(std::string vrfName);

    bool addPrefixPathV4(std::string prefix,
                         uint8_t prefixLen,
                         std::string nextHopAddress,
                         std::string nextHopIf);

    // IPv6 methods

    void setVrfV6(std::string vrfName);

    // Gribi methods
    void nextHopOp(std::string prefix,
                   uint8_t prefixLen,
                   std::string nextHopAddress,
                   std::string nextHopIf,
                   gribi::ModifyRequest modify_req);
    void nextHopGroupOp();
    void routeOp();
    void labelOp();

};


class GribiVrf {
public:
    explicit GribiVrf(std::shared_ptr<grpc::Channel> Channel);

    std::shared_ptr<grpc::Channel> channel;
};

}
