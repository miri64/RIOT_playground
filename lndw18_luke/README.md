# RIOT version

- use [255a5cd] as earliest version for fixed duplicate address detection

[255a5cd]: https://github.com/RIOT-OS/RIOT/commit/255a5cd0a60aef51fc20c7fa2c3aefca24348184

# Alternative versions

- **LCN'19 artifact repo:** https://github.com/5G-I3/coap-rd-config-demo
- **A (private) netd version with poster etc.:** https://github.com/netd-tud/dino-demo

# Config

- Channel: `DEFAULT_CHANNEL=19`
- Number of display LEDs: `LUKE_DISPLAY_CNT=64`

# Boot-up

1. Start Raspberry Pi with IEEE 802.15.4 radio connected to monitor with
   following services
    - [`lowpan`](https://github.com/RIOT-Makers/wpan-raspbian)
    - [`aiocoap-rd`](https://aiocoap.readthedocs.io/en/latest/module/aiocoap.cli.rd.html)
      listening on 6LoWPAN interface generated by `lowpan` service
    - `nginx` pointing root to `./webfrontend`
    - [`./coap_service/coap_service.py`] with `aiocoap-rd` address and
      configured root of `nginx`
2. start dino
3. start controller
4. start display
5. if all nodes appear in the web frontend the demo is basically set-up
6. Drag nodes around to set their targets

# Troubleshooting
## Widget is not movable anymore
- reload page (button is supposed to at one point)

## Node widget does not appear
- reset node manually
- if it still does not appear: reboot the whole demo (red button top right).
  Wait until the page refreshes (may take several seconds) then reset the node
  manually

## Point counter in web frontend does not react
- `¯\_(ツ)_/¯`... Reboot demo?

# Difficulty
- Difficulty (= decrement interval) can be set with the SWD0 button of th
  SAMR21-XPRO of the display
- LEDs will show the new difficulty level (the more LEDs the more difficult)
- Default difficulty is 9 of 10

# Details
## Background
- The demo is based on CoAP and CoRE-RD
- Nodes have to register their resources with the resource directory
- Nodes identify themselves to the frontend using a section of their path `<id>`
    - `/dino` => dino
    - `/btn` => controller
    - `/dsp` => display
- To configure the target of an action (button pressed, game won, ...) a node
  needs to have a POST-able target `/<id>/target`
    - Expects a JSON object with:
        - `addr`: the address + port (optional) part of a CoAP-URL (e.g.
          `[2001:db8::1]` or `[2001:db8::2]:5678`) as string
        - `path`: the path part of a CoAP-URL (e.g. `/dsp/points`) as string
    - The configured target needs to be able to handle POSTed JSON objects
      with:
        - `points`: an integer
    - The frontend expects those targets to be of the format `/<id>/points`
- All nodes should have a POST-able resource `/reboot` to reboot the node

## Current setup
Currently the demo has 3 nodes

### `controller`
- Resources:
    - `/btn/target` (POST / GET) (see [Background](#background)):
        - GET: currently configured target
        - POST: configures target of the button
    - `/reboot` (POST): reboots the node
- Sends a POST request to its configured target every 100ms
    - `points` will be the number of button presses since the last POST request

### `display`
- Resources:
    - `/dsp/points` (POST / GET):
        - GET (observable): JSON object with
            - `points`: current points counter as integer
        - POST: Expects JSON object with
            - `points`: number of points to increment points counter
    - `/dsp/target` (POST / GET) (see [Background](#background))
        - GET: currently configured target
        - POST: configures target of the button
    - `/reboot` (POST): reboots the node
- Sends a POST request to its configured target on victory condition
  (5 times received to `/dsp/points` to get points counter to the maximum)
    - `points` will be the current points counter as integer

### `dino`
- Resources:
    - `/dino/points` (POST): JSON object with
         - `points`: integer, if > 0 the dino moves
    - `/reboot` (POST): reboots the node

# Extending the demo
## Adding a node
- make sure they follow the parameters set in [Background](#background)
- extend `get_node_name()` in `webfrontend/js/luke.js` with the identifying URL
  part
- add new node name to `NODES_ORDERED` in `webfrondend/js/luke.js`
- create a `<node_name>.html.part` representing the widget in
  `webfrontend/js/luke.js`

## Adding new resources
- extend `get_resource_name()` in `webfrontend/js/luke.js`
- extend if necessary: `get_handlers`, `obs_handlers`, `post_handlers` in
  `webfrontend/js/luke.js`
- add capability for custom POST success handlers to CoAPResource.post() if
  necessary
