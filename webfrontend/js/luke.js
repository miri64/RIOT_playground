/*
 * Copyright (C) 2019 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

const POINTS_MAX = 64;
const NODES_ORDERED = ["controller", "display", "dino"];

var nodes = {};
var get_handlers = {
  "points": function (data) {
    var data = JSON.parse(data);
    var display = {y: 70, height: 182};
    var abs_height = display.height * (data.points / POINTS_MAX)
    console.log(data, abs_height)
    $("#display-points").attr("height", abs_height)
    $("#display-points").attr("y", display.y + (display.height - abs_height));
  },
  "resource-lookup": function (data) {
    var resources = parseLinkHeader(data);

    resources.forEach(function (resource) {
      if (resource.anchor in nodes) {
        nodes[resource.anchor].add_resource(resource);
        nodes[resource.anchor].is_target = false;
      }
      else {
        nodes[resource.anchor] = new CoAPNode(resource);
        nodes[resource.anchor].load_widget();
      }
    });
    load_targets();
    console.log(nodes);
  },
  "target": function (data, textStatus, jqXHR) {
    var target_node = nodes["coap://" + data.addr];
    var node = nodes[this.url.match(/.*(coap:\/\/[^\/]*)\/.*/)[1]];
    var resource = null;

    if (!target_node) {
      var match = data.addr.match(/(.*):(\d+)/)
      if (match[2] == "5683") {
        // default port was provided, maybe we know it just with the address?
        target_node = nodes["coap://" + match[1]] || { resources: [] };
      }
      else {
        // just give up
        return;
      }
    }

    for (key in target_node.resources) {
      var r = target_node.resources[key];
      if (r.url.endsWith(data.path)) {
        resource = r;
        break;
      }
    }
    // check if resource is known
    if (!resource) {
      return;
    }
    target_node.drop_to(node.name);
  },
};
var obs_handlers = {
  "points": function (evt) {
    get_handlers["points"](evt.data);
  },
  "resource-lookup": function (evt) {
    get_handlers["resource-lookup"](evt.data);
  },
};
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
  return 'undefined';
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
  return 'undefined';
}

function get_node_by_name(name) {
  for (var key in nodes) {
    if (nodes[key].name == name) {
      return nodes[key];
    }
  }
  return null;
}

function http_url_from_coap_url(url) {
  return "http://" + config.coap_service + "/coap?target=" + url;
}

function ws_url_from_coap_url(url) {
  return "ws://" + config.coap_service + "/coap_observe?target=" + url;
}

function load_targets() {
  NODES_ORDERED.forEach(function (name) {
    var node = get_node_by_name(name);
    if (!node) {
      return;
    }
    if (name == "display" && ("points" in node.resources)) {
      node.resources.points.observe();
    }
    if ("target" in node.resources) {
      node.resources.target.get();
    }
  });
}

class CoAPResource {
  static default_handler(obj) {
    console.log(obj);
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
                       CoAPResource.default_handler
    this.obs_handler = obs_handlers[this.name] ||
                       CoAPResource.default_handler;
    this.widget = null;
  }

  addr_path_pair() {
    var match = this.url.match(/coaps?:\/\/([^\/]+(:\d+)?)(\/.*)/)
    return {addr: match[1], path: match[3]};
  }

  static create_ws(url, onmessage, onerror) {
    var ws = new WebSocket(url);
    ws.onmessage = onmessage;
    if (onclose) {
      ws.onclose = onclose;
    }
    else {
      ws.onclose = function (event) {
        var ws = event.current_target;
        // just reopen after .5 seconds
        setTimeout(CoAPResource.create_ws, 500, ws.url, ws.onmessage,
                   ws.onerror);
      };
    }
  }

  get(type="json") {
    var url = http_url_from_coap_url(this.url);

    $.get(url, this.get_handler, type)
    .fail(function (jqXHR, textStatus, errorThrown) {
      console.log(jqXHR, textStatus, errorThrown)
    });
  }

  observe() {
    CoAPResource.create_ws(ws_url_from_coap_url(this.url), this.obs_handler);
  }

  post(data, type="application/json") {
    var url = http_url_from_coap_url(this.url);

    $.ajax({
      type: "POST",
      url: url,
      data: JSON.stringify(data),
      contentType: type,
      dataType: "json",
      success: function(response) {
        console.log(response)
      },
      error: function(xhr, status) {
        console.log(status + ": " + xhr)
      },
    });
  }
}

var droppable_defaults = {
  drop: function(event, ui, droppable) {
    if (!droppable) {
      droppable = $(this);
    }
    if (droppable.prop("id") == "base-droparea") {
      /* disable moving out draggable */
      droppable.removeClass("droppable");
      droppable.removeClass("ui-widget-content");
      ui.draggable.draggable("disable");
    }
    ui.helper.data('dropped', true);
    droppable.removeClass("empty-droparea");
    ui.draggable.detach().appendTo(droppable);
    ui.draggable.css({ "left": "0", "top": "0"  });
    droppable.droppable("disable");
  },
  out: function(event, ui, droppable) {
    if (!droppable) {
      droppable = $(this);
    }
    droppable.addClass("empty-droparea");
  }
}

class CoAPNode {
  constructor(resource) {
    this.anchor = resource.anchor;
    this.name = null;
    this.resources = {};
    this.add_resource(resource);
    this.widget = null;
  }

  load_widget() {
    var obj = this;
    if (!(this.witget) && ("name" in this)) {
      $.ajax("/" + this.name + ".html.part")
      .done(function(data) {
        var assetsarea = $("#assetsarea");

        obj.widget = $(data)
        var reboot_button = obj.widget.find(".reboot")
        var remove_button = obj.widget.find(".remove")
        reboot_button.click(function (event) {
          event.preventDefault();
          if ("reboot" in obj.resources) {
            obj.resources.reboot.post({});
          }
        });
        remove_button.click(function (event) {
          event.preventDefault();
          obj.remove_widget();
        });
        obj.widget.attr("title", obj.name + ": " + obj.anchor)
        if (obj.anchor.includes("[fe80::")) {
          obj.widget.addClass("bg-danger")
        }
        assetsarea.append(obj.widget);
        obj.widget.draggable({
          start: function(event, ui) {
            if ($(this).parent().hasClass("droparea")) {
              $(this).parent().droppable("enable");
            }
            ui.helper.data('dropped', false);
          },
          stop: function(event, ui) {
            if (!ui.helper.data('dropped')) {
              $(this).detach().appendTo("#assetsarea");
              $(this).css({ "left": "0", "top": "0" });
            }
          },
          snap: $("#droparea"),
          snapMode: "outer",
        })
        obj.widget.find(".droparea").droppable({
          accept: function(elem) {
            return elem.prop("id").match(/(.+)-widget/)[1] !== 'undefined';
          },
          drop: function(event, ui) {
            droppable_defaults.drop(event, ui, $(this));
            var target = get_node_by_name(
              ui.draggable.prop("id").match(/(.+)-widget/)[1]
            );
            obj.resources.target.post(
              target.resources.points.addr_path_pair()
            );
          },
          out: function(event, ui) {
            droppable_defaults.out(event, ui, $(this));
            var target = get_node_by_name(
              ui.draggable.prop("id").match(/(.+)-widget/)[1]
            );
            obj.resources.target.post({addr: "", path: ""});
          },
        });
        if (obj.name == "controller") {
          obj.drop_to("base");
          load_targets();
        }
      });
    }
  }

  remove_widget() {
    if (this.widget) {
      if (confirm("Remove " + this.name + ": " + this.anchor + " widget?")) {
        if (this.widget.parent().hasClass("droparea")) {
          this.widget.parent().droppable("enable");
          this.widget.parent().addClass("empty-droparea")
        }
        this.widget.remove();
        this.widget = null;
      }
    }
  }

  is_target() {
    return widget.parent().hasClass("droparea");
  }

  drop_to(target) {
    if (!this.widget) {
      return;
    }
    var draggable = this.widget.draggable();
    var droppable = $("#" + target + "-droparea").droppable();

    // source: https://codepen.io/arifmahmudrana/pen/ZbxrXv +
    //         http://jsfiddle.net/vwZMZ/2/
    var droppableOffset = droppable.offset(),
        draggableOffset = draggable.offset(),
        draggableHeight = draggable.height(),
        dx = droppableOffset.left - draggableOffset.left,
        dy = droppableOffset.top - draggableOffset.top - (draggableHeight / 2);

    draggable.simulate("drag", {dx: dx, dy: dy});
  }

  add_resource(resource) {
    var resource_name = get_resource_name(resource);

    if (this.name === null) {
      this.name = get_node_name(resource);
      for (var key in nodes) {
        if (nodes[key].name == this.name) {
          // remove old entry with same name
          delete nodes[key]
        }
      }
    }
    this.resources[resource_name] = new CoAPResource(resource, this,
                                                     resource_name);
  }
}

function run_luke() {
  var core_rd = null;
  $.getJSON("../coap_service.json", function(conf) {
    config = conf;
    $("#reboot-all").click(function (event) {
      if (confirm("Restart everything?")) {
        event.preventDefault();
        $.post("http://" + config.coap_service + "/reboot", function () {
          location.reload();
        });
      }
    });
    $("#reload-page").click(function (event) {
      event.preventDefault();
      location.reload();
    });
    corerd = new CoAPNode(config.corerd);
    nodes[config.corerd.anchor] = corerd;
    corerd.resources["resource-lookup"].observe()
  })
  .fail(function(jqxhr, textStatus, error) {
    var err = textStatus + ", " + error;
    console.log( "Request Failed: " + err);
  });
  $('#base-droparea').droppable(droppable_defaults);
}
