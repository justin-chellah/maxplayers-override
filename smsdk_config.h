#pragma once

#define SMEXT_CONF_NAME				"[L4D/2] Max Players Override"
#define SMEXT_CONF_DESCRIPTION		"Adds a command-line parameter for server operators to override maxplayers value, without patching bytes"
#define SMEXT_CONF_VERSION			"1.0.0"
#define SMEXT_CONF_AUTHOR			"Justin \"Sir Jay\" Chellah"
#define SMEXT_CONF_URL				"https://justin-chellah.com"
#define SMEXT_CONF_LOGTAG			"MAXPLAYERS-OVERRIDE"
#define SMEXT_CONF_LICENSE			"GPL"
#define SMEXT_CONF_DATESTRING		__DATE__
#define SMEXT_CONF_GAMEDATA_FILE	"maxplayers_override"

#define SMEXT_LINK(name) 			SDKExtension *g_pExtensionIface = name;

#define SMEXT_CONF_METAMOD

#define SMEXT_ENABLE_GAMECONF
#define SMEXT_ENABLE_MEMUTILS
#define SMEXT_ENABLE_PLAYERHELPERS