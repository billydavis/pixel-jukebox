#pragma once

// Keeps the board on Wi-Fi: joining at boot, announcing the connection, and
// rejoining when it drops. Credentials and the setup hotspot are in wifi_setup.

// Joins the saved network. With no network to join, or one that cannot be
// joined, this hands over to the setup hotspot, which ends in a restart.
void connectionStart();

// Call regularly from the display loop. Announces a new connection and starts
// the services that need one (the clock, the API); rejoins after a drop, trying
// at most once a minute so slots that work offline keep their turns.
void connectionMaintain();

// Restarts into the setup hotspot, when asked by the button or the API.
void connectionEnterSetup();
