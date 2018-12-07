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

#include <iosxrgribi/gribi.pb.h>


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

    std::map<std::string, int> prefix_map_v4;
    std::map<std::string, int> prefix_map_v6;

    // IPv4 and IPv6 string manipulation methods

    std::string longToIpv4(uint32_t nlprefix);
    uint32_t ipv4ToLong(const char* address);
    std::string ipv6ToByteArrayString(const char* address);
    std::string ByteArrayStringtoIpv6(std::string ipv6ByteArray);

    // IPv4 methods

    void setVrfV4(std::string vrfName);


    // IPv6 methods

    void setVrfV6(std::string vrfName);

};


class GribiVrf {
public:
    explicit GribiVrf(std::shared_ptr<grpc::Channel> Channel);

    std::shared_ptr<grpc::Channel> channel;
};

}
