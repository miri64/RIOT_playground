/*
 * Copyright (C) 2019 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

var nodes = {}
var get_handlers = {
  "points": function (evt) {
    // TODO
  },
  "resource-lookup": function (evt) {
    var resources = parseLinkHeader(evt.data);

    resources.forEach(function (resource) {
      if (resource.anchor in nodes) {
        nodes[resource.anchor].add_resource(resource);
      }
      else {
        nodes[resource.anchor] = new CoAPNode(resource);
      }
    });
  },
}
var config = null

function get_node_name(resource) {
  if (resource.url.includes("/btn/")) {
    return "controller";
  }
  else if (resource.url.includes("/dino/")) {
    return "dino";
  }
  else if (resource.url.endsWith("/resource-lookup")) {
    return "corerd";
  }
  else if (resource.url.includes("/dsp/")) {
    return "display";
  }
  return "undefined";
}

function get_resource_name(resource) {
  if (resource.url.endsWith("/points")) {
    return "points";
  }
  else if (resource.url.endsWith("/target")) {
    return "target";
  }
  else if (resource.url.endsWith("/reboot")) {
    return "reboot";
  }
  else if (resource.url.endsWith("/resource-lookup")) {
    return "resource-lookup";
  }
  return "undefined";
}

class CoAPResource {
  static default_get_handler(evt) {
    console.log(evt);
  }

  constructor(resource, node, name) {
    this.url = resource.url;
    this.node = node;
    if (name) {
      this.name = name;
    }
    else {
      this.name = get_resource_name(resource);
    }
    this.get_handler = get_handlers[this.name] ||
                       CoAPResource.default_get_handler;
  }

  create_ws() {
    var ws = new WebSocket("ws://" + config.coap_service +
                           "/coap_observe?target=" + this.url);
    ws.onmessage = this.get_handler;
    ws.onclose = function () {
      // just reopen after 5 seconds
      setTimeout(this.create_ws, 5000);
    };
  }

  observe() {
    this.create_ws()
  }
}

class CoAPNode {
  constructor(resource) {
    this.anchor = resource.anchor;
    this.name = null;
    this.resources = {};
    this.add_resource(resource);
  }

  add_resource(resource) {
    var resource_name = get_resource_name(resource);

    if (this.name === null) {
      this.name = get_node_name(resource);
    }
    this.resources[resource_name] = new CoAPResource(resource, this,
                                                     resource_name);
  }
}

function run_luke() {
  var core_rd = null;
  $.getJSON("../coap_service.json", function(conf) {
    config = conf
    corerd = new CoAPNode(config.corerd);
    nodes[config.corerd.url] = corerd;
    corerd.resources["resource-lookup"].observe()
  })
  .fail(function(jqxhr, textStatus, error) {
    var err = textStatus + ", " + error;
    console.log( "Request Failed: " + err);
  });
}
