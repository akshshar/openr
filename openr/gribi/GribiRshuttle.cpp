#include "GribiRshuttle.h"
#include "GribiException.h"

#include <algorithm>
#include <memory>

#include <folly/Format.h>
#include <folly/MapUtil.h>
#include <folly/Memory.h>
#include <folly/Range.h>
#include <folly/ScopeGuard.h>
#include <folly/String.h>
#include <folly/gen/Base.h>
#include <folly/gen/Core.h>

using folly::gen::as;
using folly::gen::from;
using folly::gen::mapped;

namespace {

const int kIpAddrBufSize = 1024;
//const uint32_t kAqRouteTableId = ;
} //anonymous namespace


namespace openr {

GribiRshuttle::GribiRshuttle(
            fbzmq::ZmqEventLoop* zmqEventLoop,
            std::vector<VrfData> vrfSet,
            uint8_t routeProtocolId,
            std::shared_ptr<grpc::Channel> Channel)
  : routeProtocolId_(routeProtocolId)
{
    evl_ = zmqEventLoop;
    CHECK(evl_) << "Invalid ZMQ event loop handle";

    gribiRoute_ = std::make_unique<GribiRoute>(Channel);

    evl_->runInEventLoop([this, vrfSet]() mutable {
        for (auto const &vrf_data : vrfSet) {

            // Set up the local Route DB for each vrf
            routeDb_.emplace(vrf_data.vrfName,RouteDbVrf(vrf_data));
        }
    });
}


GribiRshuttle::~GribiRshuttle()
{
    // Clear out the last vrfRegMsg batch
    //gribiVrf_->vrf_msg.clear_vrfregmsgs();

    //notifThread_.join();
}


// Set VRF context for V4 routes
void
GribiRshuttle::setGribiRouteVrfV4(std::string vrfName)
{
    gribiRoute_->setVrfV4(vrfName);
}

// Set VRF context for V6 routes
void
GribiRshuttle::setGribiRouteVrfV6(std::string vrfName)
{
    gribiRoute_->setVrfV6(vrfName);
}

// Set VRF context for V4 and V6 routes
void
GribiRshuttle::setGribiRouteVrf(std::string vrfName)
{
    LOG(INFO) << "setGribiRouteVrf";

    setGribiRouteVrfV4(vrfName);
    setGribiRouteVrfV6(vrfName);
}

std::string
GribiRshuttle::iosxrIfName(std::string ifname)
{
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

    if (iosxrLnxIfname.find(ifname.substr(0,2)) ==
            iosxrLnxIfname.end()) {
        LOG(ERROR) << "Interface type not supported";
        return "";
    }

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
        return "";
    }

    // Finally replace _ with / in ifname suffix and concatenate the
    // doctored prefix with it

    std::replace(ifname.begin(),
                 ifname.end(),
                 '_','/');


    return ifnamePrefix + ifname.substr(2, ifname.length());
}


folly::Future<folly::Unit>
GribiRshuttle::addUnicastRoute(
    const folly::CIDRNetwork& prefix, const NextHops& nextHops) {
  LOG(INFO) << "Adding unicast route";
  LOG(INFO) << "Adding unicast route";
  CHECK(not nextHops.empty());
  CHECK(not prefix.first.isMulticast() && not prefix.first.isLinkLocal());

  folly::Promise<folly::Unit> promise;
  auto future = promise.getFuture();

  evl_->runInEventLoop(
      [this, promise = std::move(promise), prefix, nextHops]() mutable {
        try {
          // In IOS-XR SL-API, we have the UPDATE utility
          // So we do NOT need to check for prefix existence
          // in current RIB. UPDATE will create a new route if
          // it doesn't exist.
          doAddUnicastRoute(prefix, nextHops);
          promise.setValue();
        } catch (GribiException const& ex) {
          LOG(ERROR) << "Error adding unicast routes to "
                     << folly::IPAddress::networkToString(prefix);
          promise.setException(ex);
        } catch (std::exception const& ex) {
          LOG(ERROR) << "Error adding unicast routes to "
                     << folly::IPAddress::networkToString(prefix);
          promise.setException(ex);
        }
      });
  return future;
}

folly::Future<folly::Unit>
GribiRshuttle::deleteUnicastRoute(const folly::CIDRNetwork& prefix) {
  LOG(INFO) << "Deleting unicast route";
  CHECK(not prefix.first.isMulticast() && not prefix.first.isLinkLocal());

  folly::Promise<folly::Unit> promise;
  auto future = promise.getFuture();

  evl_->runInEventLoop([this, promise = std::move(promise), prefix]() mutable {
    try {
/*      if (unicastRouteDb_.count(prefix) == 0) {
        LOG(ERROR) << "Trying to delete non-existing prefix "
                   << folly::IPAddress::networkToString(prefix);
      } else { */
//        const auto& oldNextHops = unicastRouteDb_.at(prefix);
        doDeleteUnicastRoute(prefix);
//        unicastRouteDb_.erase(prefix);
//      }
      promise.setValue();
    } catch (GribiException const& ex) {
      LOG(ERROR) << "Error deleting unicast routes to "
                 << folly::IPAddress::networkToString(prefix)
                 << " Error: " << folly::exceptionStr(ex);
      promise.setException(ex);
    } catch (std::exception const& ex) {
      LOG(ERROR) << "Error deleting unicast routes to "
                 << folly::IPAddress::networkToString(prefix)
                 << " Error: " << folly::exceptionStr(ex);
      promise.setException(ex);
    }
  });
  return future;
}


folly::Future<UnicastRoutes>
GribiRshuttle::getUnicastRoutes() {
  LOG(INFO) << "Getting all routes";

  folly::Promise<UnicastRoutes> promise;
  auto future = promise.getFuture();

#if 0
  evl_->runInEventLoop([this, promise = std::move(promise)]() mutable {
    try {
        promise.setValue(doGetUnicastRoutes());
    } catch (GribiException const& ex) {
      LOG(ERROR) << "Error updating route cache: " << folly::exceptionStr(ex);
      promise.setException(ex);
    } catch (std::exception const& ex) {
      LOG(ERROR) << "Error updating route cache: " << folly::exceptionStr(ex);
      promise.setException(ex);
    }
  });
#endif
  return future;
}

UnicastRoutes
GribiRshuttle::doGetUnicastRoutes() {

  LOG(INFO) << "doGetUnicastRoutes";

    // Combine the v4 and v6 maps before returning
    if (gribiRoute_->gribi_v4_op.network_instance().empty() ||
        gribiRoute_->gribi_v6_op.network_instance().empty()) {
        throw GribiException(folly::sformat(
            "VRF not set, could not fetch routes, gRIBI Error: {}",
            std::to_string(service_layer::SLErrorStatus_SLErrno_SL_RPC_ROUTE_VRF_NAME_MISSING)));
    }

    auto unicastRouteDb_ = routeDb_[gribiRoute_->gribi_v4_op.network_instance()].unicastRoutesV4_;

    unicastRouteDb_.insert(routeDb_[gribiRoute_->gribi_v6_op.network_instance()].unicastRoutesV6_.begin(),
                           routeDb_[gribiRoute_->gribi_v6_op.network_instance()].unicastRoutesV6_.end());

    return unicastRouteDb_;
}

folly::Future<folly::Unit>
GribiRshuttle::syncRoutes(UnicastRoutes newRouteDb) {

    //LOG(INFO) << "Syncing Routes...";

    folly::Promise<folly::Unit> promise;
    auto future = promise.getFuture();

    evl_->runInEventLoop(
      [this, promise = std::move(promise), newRouteDb]() mutable {
        try {
          doSyncRoutes(newRouteDb);
          promise.setValue();
        } catch (GribiException const& ex) {
          LOG(ERROR) << "Error syncing routeDb with Fib: "
                     << folly::exceptionStr(ex);
          promise.setException(ex);
        } catch (std::exception const& ex) {
          LOG(ERROR) << "Error syncing routeDb with Fib: "
                     << folly::exceptionStr(ex);
          promise.setException(ex);
        }
      });

    return future;
}

void
GribiRshuttle::doAddUnicastRoute(const folly::CIDRNetwork& prefix,
                                 const NextHops& nextHops) {

    LOG(INFO) << "doAddUnicastRoute....";

    if (prefix.first.isV4()) {
        if (gribiRoute_->gribi_v4_op.network_instance().empty()) {
            throw GribiException(folly::sformat(
                "Could not add Route to: {} gRIBI Error: {}",
                folly::IPAddress::networkToString(prefix),
                std::to_string(service_layer::SLErrorStatus_SLErrno_SL_RPC_ROUTE_VRF_NAME_MISSING)));
        }

        routeDb_[gribiRoute_->
                 gribi_v4_op.network_instance()].unicastRoutesV4_[prefix] = nextHops;
        doAddUnicastRouteV4(prefix, nextHops);
    } else {
        if (gribiRoute_->gribi_v6_op.network_instance().empty()) {
            throw GribiException(folly::sformat(
                "Could not add Route to: {} gRIBI Error: {}",
                folly::IPAddress::networkToString(prefix),
                std::to_string(service_layer::SLErrorStatus_SLErrno_SL_RPC_ROUTE_VRF_NAME_MISSING)));
        }
        routeDb_[gribiRoute_->
                 gribi_v6_op.network_instance()].unicastRoutesV6_[prefix] = nextHops;
        doAddUnicastRouteV6(prefix, nextHops);
    }

    // Cache new nexthops in our local-cache if everything is good
    // unicastRoutes_[prefix].insert(nextHops.begin(), nextHops.end());

}


void
GribiRshuttle::doAddUnicastRouteV4(
    const folly::CIDRNetwork& prefix, const NextHops& nextHops) {

    CHECK(prefix.first.isV4());

    LOG(INFO) << "Prefix Received: " << folly::IPAddress::networkToString(prefix);

    for (auto const& nextHop : nextHops) {
        CHECK(nextHop.second.isV4());
        LOG(INFO) << "Nexthop : "<< std::get<1>(nextHop).str() << ", " << std::get<0>(nextHop).c_str();
        // Create a path list

        auto nexthop_if = iosxrIfName(std::get<0>(nextHop));
        auto nexthop_address = std::get<1>(nextHop).str();

        //gribiRoute_->insertAddBatchV4(prefix.first.str(),
                                      //folly::to<uint8_t>(prefix.second),
                                      //routeProtocolId_,
                                      //nexthop_address,
                                      //nexthop_if);
      }

      // Using the Update Operation to replace an existing prefix
      // or create one if it doesn't exist.
      //auto result  = gribiRoute_->routev4Op(service_layer::SL_OBJOP_UPDATE);
      //if (!result) {
          //throw GribiException(folly::sformat("Could not add Route to: {}",
                               //folly::IPAddress::networkToString(prefix)));
      //}
}

void
GribiRshuttle::doAddUnicastRouteV6(
    const folly::CIDRNetwork& prefix, const NextHops& nextHops) {

  CHECK(prefix.first.isV6());
  for (auto const& nextHop : nextHops) {
    CHECK(nextHop.second.isV6());
  }

  LOG(INFO) << "Prefix Received: " << folly::IPAddress::networkToString(prefix);


#if 0
  for (auto const& nextHop : nextHops) {
    CHECK(nextHop.second.isV6());
    LOG(INFO) << "Nexthop : "<< std::get<1>(nextHop).str() << ", " << std::get<0>(nextHop).c_str();
    // Create a path list

    auto nexthop_if = iosxrIfName(std::get<0>(nextHop));
    auto nexthop_address = std::get<1>(nextHop).str();

    gribiRoute_->insertAddBatchV6(prefix.first.str(),
                                    folly::to<uint8_t>(prefix.second),
                                    routeProtocolId_,
                                    nexthop_address,
                                    nexthop_if);
  }

  // Using the Update Operation to replace an existing prefix
  // or create one if it doesn't exist.
  auto result  = gribiRoute_->routev6Op(service_layer::SL_OBJOP_UPDATE);
  if (!result) {
    throw GribiException(folly::sformat(
        "Could not add Route to: {}",
        folly::IPAddress::networkToString(prefix)));
  }
#endif
}

void
GribiRshuttle::doDeleteUnicastRoute(
                    const folly::CIDRNetwork& prefix) {

  LOG(INFO) << "doDeleteUnicastRoute....";

#if 0
  if (prefix.first.isV4()) {
    if(gribiRoute_->
         routev4_msg.vrfname().empty()) {
        throw GribiException(folly::sformat(
            "Could not delete prefix: {} Service Layer Error: {}",
            folly::IPAddress::networkToString(prefix),
            std::to_string(service_layer::SLErrorStatus_SLErrno_SL_RPC_ROUTE_VRF_NAME_MISSING)));
    }
    routeDb_[gribiRoute_->
               routev4_msg.vrfname()].unicastRoutesV4_.erase(prefix);
    deleteUnicastRouteV4(prefix);
  } else {
    if(gribiRoute_->
         routev6_msg.vrfname().empty()) {
        throw GribiException(folly::sformat(
            "Could not delete prefix: {} Service Layer Error: {}",
            folly::IPAddress::networkToString(prefix),
            std::to_string(service_layer::SLErrorStatus_SLErrno_SL_RPC_ROUTE_VRF_NAME_MISSING)));
    }
    routeDb_[gribiRoute_->
               routev6_msg.vrfname()].unicastRoutesV6_.erase(prefix);

    deleteUnicastRouteV6(prefix);
  }
#endif
}


void
GribiRshuttle::deleteUnicastRouteV4(
                  const folly::CIDRNetwork& prefix) {
  CHECK(prefix.first.isV4());

  LOG(INFO) << "deleteUnicastRouteV4....";

#if 0
  gribiRoute_->insertDeleteBatchV4(prefix.first.str(),
                                  folly::to<uint8_t>(prefix.second));


  auto result = gribiRoute_->routev4Op(service_layer::SL_OBJOP_DELETE);

  if (!result) {
    throw GribiException(folly::sformat(
        "Failed to delete route {}",
        folly::IPAddress::networkToString(prefix)));
  }
#endif
}

void
GribiRshuttle::deleteUnicastRouteV6(
                  const folly::CIDRNetwork& prefix) {
  CHECK(prefix.first.isV6());

  LOG(INFO) << "deleteUnicastRouteV6....";

#if 0
  gribiRoute_->insertDeleteBatchV6(prefix.first.str(),
                                  folly::to<uint8_t>(prefix.second));


  auto result = gribiRoute_->routev6Op(service_layer::SL_OBJOP_DELETE);

  if (!result) {
    throw GribiException(folly::sformat(
        "Failed to delete route {}",
        folly::IPAddress::networkToString(prefix)));
  }
#endif

}


void
GribiRshuttle::doSyncRoutes(UnicastRoutes newRouteDb) {

  LOG(INFO) << "doSyncRoutes....";

  // Fetch the latest Application RIB state
  auto unicastRouteDb_ = doGetUnicastRoutes();

  // Go over routes that are not in new routeDb, delete
  for (auto it = unicastRouteDb_.begin(); it != unicastRouteDb_.end();) {
    auto const& prefix = it->first;
    if (newRouteDb.find(prefix) == newRouteDb.end()) {
      try {
        doDeleteUnicastRoute(prefix);
      } catch (GribiException const& err) {
        LOG(ERROR) << folly::sformat(
            "Could not del Route to: {} Error: {}",
            folly::IPAddress::networkToString(prefix),
            folly::exceptionStr(err));
      } catch (std::exception const& err) {
        LOG(ERROR) << folly::sformat(
            "Could not del Route to: {} Error: {}",
            folly::IPAddress::networkToString(prefix),
            folly::exceptionStr(err));
      }
      it = unicastRouteDb_.erase(it);
    } else {
      ++it;
    }
  }

  // Using the gribi Modify operation
  // simply push the newRoutedb into the XR RIB

  for (auto const& kv : newRouteDb) {
      auto const& prefix = kv.first;
      try {
        doAddUnicastRoute(prefix, kv.second);
      } catch (GribiException const& err) {
        LOG(ERROR) << folly::sformat(
            "Could not add Route to: {} Error: {}",
            folly::IPAddress::networkToString(prefix),
            folly::exceptionStr(err));
      } catch (std::exception const& err) {
        LOG(ERROR) << folly::sformat(
            "Could not add Route to: {} Error: {}",
            folly::IPAddress::networkToString(prefix),
            folly::exceptionStr(err));
      }
      unicastRouteDb_.emplace(prefix, std::move(kv.second));
  }
}


}
