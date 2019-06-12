/*
 * Copyright (C) 2019 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

function run_luke() {
  var core_rd = null;
  $.getJSON("../coap_service.json", function(config) {
    corerd = new WebSocket("ws://" + config.coap_service +
                           "/coap_observe?target=" + config.corerd.url);
    corerd.onmessage = function (evt) {
      console.log(parseLinkHeader(evt.data));
    }
  })
    .fail(function(jqxhr, textStatus, error) {
      var err = textStatus + ", " + error;
      console.log( "Request Failed: " + err  );
    });
}
