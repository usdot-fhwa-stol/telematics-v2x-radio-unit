#!/bin/bash
# Use MYSQL_HOSTNAME environment variable, default to tru_db for dev container
MYSQL_HOST=${MYSQL_HOSTNAME:-tru_db}
wait-for-it.sh $MYSQL_HOST:3306

# Add stol apt repository libraries to path for tmxcore
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/opt/carma/lib/
for plugin in /usr/local/plugins/*.zip; do
    echo "Installing plugin $plugin"
    tmxctl --host "$MYSQL_HOST" --plugin-install "$plugin"
done

echo "TelematicBridge Configuration:"
echo "  RSU_CONFIG_PATH: $RSU_CONFIG_PATH"
echo "  NATS_URL: $NATS_URL"
echo "  IS_TRU: $IS_TRU"

# Start Tmx Core in background
echo "Starting TMX Core..."
tmxcore &
TMXCORE_PID=$!

# Wait for tmxcore to be ready
echo "Waiting for TMX Core to initialize..."
sleep 5

# Enable required plugins for TelematicRSU Unit
echo "Configuring plugins for RSU data streaming..."

# Now enable the required plugins
echo "Enabling MessageReceiver RSUHealthMonitor TelematicBridge..."
tmxctl --host "$MYSQL_HOST" --plugin MessageReceiver --enable
tmxctl --host "$MYSQL_HOST" --plugin RSUHealthMonitor --enable
tmxctl --host "$MYSQL_HOST" --plugin TelematicBridge --enable

# Wait for tmxcore process
wait $TMXCORE_PID
