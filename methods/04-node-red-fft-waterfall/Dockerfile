# Standalone Node-RED for Method 4 (FFT / waterfall).
# Adds the dashboard palette (ui_chart / ui_template / ui_dropdown) on top of core
# Node-RED. Core nodes used (mqtt in, function, file in, json) need no install.
FROM nodered/node-red:4.0

# Dashboard 1.x provides the ui_* nodes referenced by flow.json.
RUN npm install --no-audit --no-fund node-red-dashboard@3
