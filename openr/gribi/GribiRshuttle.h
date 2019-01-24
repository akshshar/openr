#pragma once

#include "GribiRoute.h"

#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <fbzmq/async/ZmqEventLoop.h>
#include <fbzmq/async/ZmqTimeout.h>
#include <fbzmq/zmq/Zmq.h>
#include <folly/IPAddress.h>
#include <folly/futures/Future.h>
#include <openr/common/AddressUtil.h>
#include <openr/common/Util.h>

// Only need (?) routeDb and vrfData
#include <openr/iosxrsl/ServiceLayerRshuttle.h>


namespace openr {

// nextHop => local interface and nextHop IP.
using NextHops = std::unordered_set<std::pair<std::string, folly::IPAddress>>;

// Route => prefix and its possible nextHops
using UnicastRoutes = std::unordered_map<folly::CIDRNetwork, NextHops>;

using RouteDb = std::unordered_map<std::string, RouteDbVrf >;

class GribiRshuttle {
public:
    explicit GribiRshuttle(
            fbzmq::ZmqEventLoop* zmqEventLoop,
            uint8_t routeProtocolId,
            std::shared_ptr<grpc::Channel> Channel);

    //~GribiRshuttle();

  // If adding multipath nextHops for the same prefix at different times,
  // always provide unique nextHops and not cumulative list.

  folly::Future<folly::Unit> addUnicastRoute(
      const folly::CIDRNetwork& prefix, const NextHops& nextHops);

  // Delete all next hops associated with prefix
  folly::Future<folly::Unit> deleteUnicastRoute(
      const folly::CIDRNetwork& prefix);

  // Sync route table in IOS-XR RIB with given route table
  // Basically when there's mismatch between IOS-XR RIB and route table in
  // application, we sync RIB with given data source
  folly::Future<folly::Unit> syncRoutes(UnicastRoutes newRouteDb);

  // get cached unicast routing table
  folly::Future<UnicastRoutes> getUnicastRoutes();

  // Set VRF context for V4 routes
  void setGribiRouteVrfV4(std::string vrfName);

  // Set VRF context for V6 routes
  void setGribiRouteVrfV6(std::string vrfName);

  // Set VRF context for V4 and V6 routes
  void setGribiRouteVrf(std::string vrfName);

  // Method to convert IOS-XR Linux interface names to IOS-XR Interface names
  std::string
  iosxrIfName(std::string ifname);

 private:
  GribiRshuttle(const GribiRshuttle&) = delete;
  GribiRshuttle& operator=(const GribiRshuttle&) = delete;

  /**
   * Specific implementation for adding v4 and v6 routes into IOS-XR RIB. This
   * is because so that we can have consistent APIs to user even though IOS-XR SL
   * has different behaviour for ipv4 and ipv6 route APIs.
   *
   */
  void doAddUnicastRoute(
      const folly::CIDRNetwork& prefix, const NextHops& nextHops);
  void doAddUnicastRouteV4(
      const folly::CIDRNetwork& prefix, const NextHops& nextHops);
  void doAddUnicastRouteV6(
      const folly::CIDRNetwork& prefix, const NextHops& nextHops);

  void doDeleteUnicastRoute(
      const folly::CIDRNetwork& prefix);
  void deleteUnicastRouteV4(
      const folly::CIDRNetwork& prefix);
  void deleteUnicastRouteV6(
      const folly::CIDRNetwork& prefix);

  UnicastRoutes doGetUnicastRoutes();

  void doSyncRoutes(UnicastRoutes newRouteDb);

  fbzmq::ZmqEventLoop* evl_{nullptr};
  std::thread notifThread_;
  //std::shared_ptr<AsyncNotifChannel> asynchandler_;
  std::unique_ptr<GribiRoute> gribiRoute_;
  std::unique_ptr<GribiVrf> gribiVrf_;

  const uint8_t routeProtocolId_{0};

  // With gRIBI APIs, openR is just another protocol on the
  // IOS-XR system. We maintain a local Route Database for sanity checks
  // and quicker route queries.
  RouteDb routeDb_;

};

}
