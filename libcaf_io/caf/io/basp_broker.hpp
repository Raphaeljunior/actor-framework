/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <chrono>
#include <future>
#include <map>
#include <set>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/detail/io_export.hpp"
#include "caf/forwarding_actor_proxy.hpp"
#include "caf/io/basp/all.hpp"
#include "caf/io/broker.hpp"
#include "caf/io/typed_broker.hpp"
#include "caf/proxy_registry.hpp"
#include "caf/stateful_actor.hpp"

namespace caf::io {

/// A broker implementation for the Binary Actor System Protocol (BASP).
class CAF_IO_EXPORT basp_broker : public broker,
                                  public proxy_registry::backend,
                                  public basp::instance::callee {
public:
  // -- member types -----------------------------------------------------------

  using super = broker;

  using ctx_map = std::unordered_map<connection_handle, basp::endpoint_context>;

  using monitored_actor_map
    = std::unordered_map<actor_addr, std::unordered_set<node_id>>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit basp_broker(actor_config& cfg);

  ~basp_broker() override;

  // -- implementation of broker -----------------------------------------------

  void on_exit() override;

  const char* name() const override;

  behavior make_behavior() override;

  proxy_registry* proxy_registry_ptr() override;

  resume_result resume(execution_unit*, size_t) override;

  // -- implementation of proxy_registry::backend ------------------------------

  strong_actor_ptr make_proxy(node_id nid, actor_id aid) override;

  void set_last_hop(node_id* ptr) override;

  // -- implementation of basp::instance::callee -------------------------------

  void finalize_handshake(const node_id& nid, actor_id aid,
                          std::set<std::string>& sigs) override;

  void purge_state(const node_id& nid) override;

  void proxy_announced(const node_id& nid, actor_id aid) override;

  void learned_new_node_directly(const node_id& nid,
                                 bool was_indirectly_before) override;

  void learned_new_node_indirectly(const node_id& nid) override;

  byte_buffer& get_buffer(connection_handle hdl) override;

  void flush(connection_handle hdl) override;

  void handle_heartbeat() override;

  execution_unit* current_execution_unit() override;

  strong_actor_ptr this_actor() override;

  // -- utility functions ------------------------------------------------------

  /// Sends `node_down_msg` to all registered observers.
  void emit_node_down_msg(const node_id& node, const error& reason);

  /// Performs bookkeeping such as managing `spawn_servers`.
  void learned_new_node(const node_id& nid);

  /// Sets `this_context` by either creating or accessing state for `hdl`.
  void set_context(connection_handle hdl);

  /// Cleans up any state for `hdl`.
  void connection_cleanup(connection_handle hdl, sec code);

  /// Sends a basp::down_message message to a remote node.
  void send_basp_down_message(const node_id& nid, actor_id aid, error err);

  // Sends basp::down_message to all nodes monitoring the terminated actor.
  void handle_down_msg(down_msg&);

  // -- disambiguation for functions found in multiple base classes ------------

  actor_system& system() {
    return super::system();
  }

  const actor_system_config& config() {
    return system().config();
  }

  // -- member variables -------------------------------------------------------

  /// Protocol instance of BASP.
  union {
    basp::instance instance;
  };

  /// Keeps context information for all open connections.
  ctx_map ctx;

  /// points to the current context for callbacks.
  basp::endpoint_context* this_context;

  /// Stores handles to spawn servers for other nodes. These servers are
  /// spawned whenever the broker learns a new node ID and tries to get a
  /// 'SpawnServ' instance on the remote side.
  std::unordered_map<node_id, actor> spawn_servers;

  /// Keeps track of actors that wish to receive a `node_down_msg` if a
  /// particular node fails.
  std::unordered_map<node_id, std::vector<actor_addr>> node_observers;

  /// Configures whether BASP automatically open new connections to optimize
  /// routing paths by forming a mesh between all nodes.
  bool automatic_connections = false;

  /// Returns the node identifier of the underlying BASP instance.
  const node_id& this_node() const {
    return instance.this_node();
  }

  /// Keeps track of nodes that monitor local actors.
  monitored_actor_map monitored_actors;

  std::vector<std::chrono::microseconds> dequeue_ts_;
};

} // namespace caf::io
