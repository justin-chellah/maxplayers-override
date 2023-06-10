#include "extension.h"
#include "tier0/icommandline.h"
#include "../imatchext/IMatchExtInterface.h"
#include <vstdlib/IKeyValuesSystem.h>
#include "convardata.h"

CMaxPlayersOverrideExt g_MaxPlayersOverrideExt;

SMEXT_LINK(&g_MaxPlayersOverrideExt);

IMatchExtInterface *imatchext = nullptr;
IServer *server = nullptr;
void *g_pGameRules = nullptr;
ConVar *sv_allow_lobby_connect_only = nullptr;

bool g_bAllowLobbyConnectOnly = false;

SH_DECL_HOOK1_void(IServerGameDLL, ApplyGameSettings, SH_NOATTRIB, 0, KeyValues *);
SH_DECL_HOOK1_void(IServerGameDLL, ServerHibernationUpdate, SH_NOATTRIB, false, bool);
SH_DECL_MANUALHOOK0(MHook_CGameRules_GetMaxHumanPlayers, 0, 0, 0, int);
SH_DECL_MANUALHOOK1_void(MHook_CMultiplayRules_ClientDisconnected, 0, 0, 0, edict_t *);
SH_DECL_MANUALHOOK0(MHook_CMatchTitle_GetTotalNumPlayersSupported, 0, 0, 0, int);

void OnAllowLobbyConnectOnlyChanged(IConVar *var, const char *pOldValue, float flOldValue)
{
	g_bAllowLobbyConnectOnly = sv_allow_lobby_connect_only->GetBool();
}

static bool HasPlayerControlledZombies()
{
	static ConVarRef mp_gamemode("mp_gamemode");

	const char *pszName = mp_gamemode.GetString();

#if SOURCE_ENGINE == SE_LEFT4DEAD2
	KeyValues *pModeInfo = imatchext->GetIMatchExtL4D()->GetGameModeInfo(pszName);

	if (pModeInfo)
	{
		return pModeInfo->GetInt("playercontrolledzombies") > 0;
	}

	return false;
#else
	return !V_stricmp(pszName, "versus") || !V_stricmp(pszName, "teamversus");
#endif
}

bool CMaxPlayersOverrideExt::SDK_OnLoad(char *error, size_t maxlen, bool late)
{
	IGameConfig *pGameConfig;
	if (!gameconfs->LoadGameConfigFile(SMEXT_CONF_GAMEDATA_FILE, &pGameConfig, error, sizeof(error)))
	{
		ke::SafeStrcpy(error, maxlen, "Unable to load gamedata file \"" SMEXT_CONF_GAMEDATA_FILE ".txt\"");

		return false;
	}

	int iVtbl_CGameRules_GetMaxHumanPlayers = -1;

	if (!pGameConfig->GetOffset("CGameRules::GetMaxHumanPlayers", &iVtbl_CGameRules_GetMaxHumanPlayers))
	{
		ke::SafeStrcpy(error, maxlen, "Unable to find gamedata offset entry for \"CGameRules::GetMaxHumanPlayers\"");

		gameconfs->CloseGameConfigFile(pGameConfig);

		return false;
	}

	int iVtbl_CMultiplayRules_ClientDisconnected = -1;

	if (!pGameConfig->GetOffset("CMultiplayRules::ClientDisconnected", &iVtbl_CMultiplayRules_ClientDisconnected))
	{
		ke::SafeStrcpy(error, maxlen, "Unable to find gamedata offset entry for \"CMultiplayRules::GetMaxHumanPlayers\"");

		gameconfs->CloseGameConfigFile(pGameConfig);

		return false;
	}

	int iVtbl_CMatchTitle_GetTotalNumPlayersSupported = -1;

	if (!pGameConfig->GetOffset("CMatchTitle::GetTotalNumPlayersSupported", &iVtbl_CMatchTitle_GetTotalNumPlayersSupported))
	{
		ke::SafeStrcpy(error, maxlen, "Unable to find gamedata offset entry for \"CMatchTitle::GetTotalNumPlayersSupported\"");

		gameconfs->CloseGameConfigFile(pGameConfig);

		return false;
	}

	SH_MANUALHOOK_RECONFIGURE(MHook_CGameRules_GetMaxHumanPlayers, iVtbl_CGameRules_GetMaxHumanPlayers, 0, 0);
	SH_MANUALHOOK_RECONFIGURE(MHook_CMultiplayRules_ClientDisconnected, iVtbl_CMultiplayRules_ClientDisconnected, 0, 0);
	SH_MANUALHOOK_RECONFIGURE(MHook_CMatchTitle_GetTotalNumPlayersSupported, iVtbl_CMatchTitle_GetTotalNumPlayersSupported, 0, 0);

	if (!pGameConfig->GetMemSig("CBaseServer::SetReservationCookie", &m_pfn_CBaseServer_SetReservationCookie))
	{
		ke::SafeStrcpy(error, maxlen, "Unable to find gamedata address entry for \"CBaseServer::SetReservationCookie\"");

		gameconfs->CloseGameConfigFile(pGameConfig);

		return false;
	}

	if (m_pfn_CBaseServer_SetReservationCookie == nullptr)
	{
		ke::SafeStrcpy(error, maxlen, "Unable to find signature in binary for gamedata entry \"CBaseServer::SetReservationCookie\"");

		gameconfs->CloseGameConfigFile(pGameConfig);

		return false;
	}

	gameconfs->CloseGameConfigFile(pGameConfig);

	playerhelpers->AddClientListener(this);

	if (late && playerhelpers->IsServerActivated())
	{
		OnServerActivated(playerhelpers->GetMaxClients());
	}

	sharesys->AddDependency(myself, "bintools.ext", true, true);
	sharesys->AddDependency(myself, "imatchext.ext", true, true);

	SH_ADD_HOOK(IServerGameDLL, ApplyGameSettings, gamedll, SH_MEMBER(this, &CMaxPlayersOverrideExt::ApplyGameSettings), false);
	SH_ADD_HOOK(IServerGameDLL, ServerHibernationUpdate, gamedll, SH_MEMBER(this, &CMaxPlayersOverrideExt::ServerHibernationUpdate), false);

	sv_allow_lobby_connect_only = g_pCVar->FindVar("sv_allow_lobby_connect_only");
	sv_allow_lobby_connect_only->InstallChangeCallback(OnAllowLobbyConnectOnlyChanged);

	m_bLate = late;

	return true;
}

void CMaxPlayersOverrideExt::SDK_OnUnload()
{
	playerhelpers->RemoveClientListener(this);

	ConVarData *pConVarData = reinterpret_cast<ConVarData *>(sv_allow_lobby_connect_only)->m_pParent;
	pConVarData->m_fnChangeCallback = nullptr;

	SH_REMOVE_HOOK(IServerGameDLL, ApplyGameSettings, gamedll, SH_MEMBER(this, &CMaxPlayersOverrideExt::ApplyGameSettings), false);
	SH_REMOVE_HOOK(IServerGameDLL, ServerHibernationUpdate, gamedll, SH_MEMBER(this, &CMaxPlayersOverrideExt::ServerHibernationUpdate), false);

	SH_REMOVE_HOOK_ID(m_nSHookID_CMatchTitle_GetTotalNumPlayersSupported);
	m_nSHookID_CMatchTitle_GetTotalNumPlayersSupported = -1;

	CleanupGameRulesHooks();
}

void CMaxPlayersOverrideExt::SDK_OnAllLoaded()
{
	SM_GET_LATE_IFACE(BINTOOLS, m_pBinTools);
	SM_GET_LATE_IFACE(SDKTOOLS, m_pSDKTools);
	SM_GET_LATE_IFACE(IMATCHEXT, imatchext);

	server = m_pSDKTools->GetIServer();

	IMatchTitle *pMatchTitle = imatchext->GetIMatchFrameWork()->GetMatchTitle();

	m_nSHookID_CMatchTitle_GetTotalNumPlayersSupported = SH_ADD_MANUALVPHOOK(
		MHook_CMatchTitle_GetTotalNumPlayersSupported,
		pMatchTitle,
		SH_MEMBER(this, &CMaxPlayersOverrideExt::GetTotalNumPlayersSupported), false);

	if (m_bLate)
	{
		SetupGameRulesHooks();
		TryClearReservationCookie();

		m_bLate = false;
	}
}

bool CMaxPlayersOverrideExt::SDK_OnMetamodLoad(ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);

	return true;
}

bool CMaxPlayersOverrideExt::QueryRunning(char *error, size_t maxlength)
{
	SM_CHECK_IFACE(BINTOOLS, m_pBinTools);
	SM_CHECK_IFACE(SDKTOOLS, m_pSDKTools);
	SM_CHECK_IFACE(IMATCHEXT, imatchext);

	return true;
}

// Necessary so players are able to join when server with modified max players value is full and reserved
void CMaxPlayersOverrideExt::OnClientConnected(int client)
{
	IGamePlayer *pPlayer = playerhelpers->GetGamePlayer(client);

	if (pPlayer->IsFakeClient())
	{
		return;
	}

	TryClearReservationCookie();
}

void CMaxPlayersOverrideExt::OnServerActivated(int max_clients)
{
	m_iMaxPlayers = max_clients;
}

// g_pGameRules object isn't available until CWorld::Precache was called
void CMaxPlayersOverrideExt::OnCoreMapStart(edict_t *pEdictList, int edictCount, int clientMax)
{
	SetupGameRulesHooks();
}

// g_pGameRules object is destroyed when the map ends
void CMaxPlayersOverrideExt::OnCoreMapEnd()
{
	CleanupGameRulesHooks();
}

int CMaxPlayersOverrideExt::GetMaxPlayersOverrideCmdLineValue() const
{
	constexpr int nDefaultVal = -1;

	int nNewMaxPlayers = CommandLine()->ParmValue("-maxplayers_override", nDefaultVal);

	if (nNewMaxPlayers > m_iMaxPlayers)
	{
	   nNewMaxPlayers = m_iMaxPlayers;
	}

	return nNewMaxPlayers;
}

int CMaxPlayersOverrideExt::CountHumanPlayers() const
{
	int nPlayers = 0;

	for (int iClient = 1; iClient <= m_iMaxPlayers; iClient++)
	{
		IGamePlayer *pPlayer = playerhelpers->GetGamePlayer(iClient);

		if (!pPlayer
			|| !pPlayer->IsConnected()
			|| pPlayer->IsFakeClient())
		{
			continue;
		}

		nPlayers++;
	}

	return nPlayers;
}

void CMaxPlayersOverrideExt::SetupGameRulesHooks()
{
	g_pGameRules = m_pSDKTools->GetGameRules();

	m_nSHookID_CGameRules_GetMaxHumanPlayers = SH_ADD_MANUALVPHOOK(
		MHook_CGameRules_GetMaxHumanPlayers,
		g_pGameRules,
		SH_MEMBER(this, &CMaxPlayersOverrideExt::GetMaxHumanPlayers), false);

	m_nSHookID_CMultiplayRules_ClientDisconnected = SH_ADD_MANUALVPHOOK(
		MHook_CMultiplayRules_ClientDisconnected,
		g_pGameRules,
		SH_MEMBER(this, &CMaxPlayersOverrideExt::ClientDisconnected), false);
}

void CMaxPlayersOverrideExt::CleanupGameRulesHooks()
{
	SH_REMOVE_HOOK_ID(m_nSHookID_CGameRules_GetMaxHumanPlayers);
	m_nSHookID_CGameRules_GetMaxHumanPlayers = -1;

	SH_REMOVE_HOOK_ID(m_nSHookID_CMultiplayRules_ClientDisconnected);
	m_nSHookID_CMultiplayRules_ClientDisconnected = -1;
}

void CMaxPlayersOverrideExt::SetReservationCookie(uint64_t ullCookie, char const *pszReasonFormat, ...) const
{
	static ICallWrapper *pWrapper = nullptr;

	if (pWrapper == nullptr)
	{
		PassInfo params[] =
		{
			#if defined _WIN32
			{ PassType_Basic, PASSFLAG_BYVAL, sizeof(IServer *), nullptr, 0 },
			#endif
			{ PassType_Basic, PASSFLAG_BYVAL, sizeof(uint64_t), nullptr, 0 },
			{ PassType_Basic, PASSFLAG_BYVAL, sizeof(char const *), nullptr, 0 },
			{ PassType_Basic, PASSFLAG_BYVAL, sizeof(va_list *), nullptr, 0 },
		};

		#if defined _WIN32
		pWrapper = m_pBinTools->CreateCall(m_pfn_CBaseServer_SetReservationCookie, CallConv_Cdecl, nullptr, params, 4);
		#else
		pWrapper = m_pBinTools->CreateCall(m_pfn_CBaseServer_SetReservationCookie, CallConv_ThisCall, nullptr, params, 3);
		#endif
	}

	char szReason[256] = {0};
	va_list argptr;
	va_start(argptr, pszReasonFormat);
	ke::SafeVsprintf(szReason, sizeof(szReason), pszReasonFormat, argptr);
	va_end(argptr);

	#pragma pack(push, 1)
	struct
	{
		IServer *server;
		uint64_t ullCookie;
		char const *pszReasonFormat;
		va_list argptr;
	}
	#pragma pack(pop)
	stStack =
	{
		server,
		ullCookie,
		szReason,
		argptr
	};

	pWrapper->Execute(&stStack, nullptr);
}

void CMaxPlayersOverrideExt::TryClearReservationCookie() const
{
	uint64_t nReservationCookie;
	imatchext->GetReservationCookie(nReservationCookie);

	if (nReservationCookie != 0)
	{
		const int nMaxPlayers = GetMaxPlayersOverrideCmdLineValue();
		int nMaxPlayersLobby;

		constexpr int nMaxPlayersPvE = 4;
		constexpr int nMaxPlayersPvP = 8;

		if (HasPlayerControlledZombies())
		{
			nMaxPlayersLobby = nMaxPlayersPvP;
		}
		else
		{
			nMaxPlayersLobby = nMaxPlayersPvE;
		}

		if (nMaxPlayers > nMaxPlayersLobby)
		{
			int nHumanPlayers = CountHumanPlayers();

			// This allows players to join the server despite it "being full" (lobby is limited to 4 or 8, depending on game mode configuration)
			if (nHumanPlayers == nMaxPlayersLobby)
			{
				SetReservationCookie(0, "[" SMEXT_CONF_LOGTAG "] Maximum amount of players for a lobby reached (%d)", nMaxPlayersLobby);

				// Don't trigger callback so we know what the server operator had set initially
				ConVarData *pConVarData = reinterpret_cast<ConVarData *>(sv_allow_lobby_connect_only)->m_pParent;
				pConVarData->m_fnChangeCallback = nullptr;
				sv_allow_lobby_connect_only->SetValue(false);
				pConVarData->m_fnChangeCallback = OnAllowLobbyConnectOnlyChanged;
			}
		}
	}
}

void CMaxPlayersOverrideExt::ApplyGameSettings(KeyValues *pRequest)
{
	SET_META_RESULT(MRES_IGNORED);

	if (pRequest)
	{
		int nMaxPlayers = g_MaxPlayersOverrideExt.GetMaxPlayersOverrideCmdLineValue();

		if (nMaxPlayers > 0)
		{
			if (g_bAllowLobbyConnectOnly)
			{
				const int nSlots = pRequest->GetInt("Members/numSlots");

				// Some players won't be able to join if the server has set fewer maximum players than the lobby
				if (nMaxPlayers < nSlots)
				{
					smutils->LogError(
						myself,
						"Max players should not be set lower than the limit of a lobby when running the server with \"sv_allow_lobby_connect_only\" enabled");

					// Reset this since this is a critical error
					CommandLine()->RemoveParm("-maxplayers_override");
				}
			}
			else
			{
				SetReservationCookie(0, "[" SMEXT_CONF_LOGTAG "] Server has \"sv_allow_lobby_connect_only\" ConVar set to 0");
			}
		}
	}
}

void CMaxPlayersOverrideExt::ServerHibernationUpdate(bool bHibernating)
{
	if (bHibernating)
	{
		// Revert only if the server operator had changed this cvar beforehand
		if (g_bAllowLobbyConnectOnly)
		{
			sv_allow_lobby_connect_only->Revert();
		}
	}
}

// Cleanups should never occur on the player_disconnect event. It's not reliable/doesn't always fire,
// not to mention game SDK does it the same way
void CMaxPlayersOverrideExt::ClientDisconnected(edict_t *pClient)
{
	// Revert only if the server operator had changed this cvar beforehand
	if (!g_bAllowLobbyConnectOnly)
	{
		return;
	}

	const int nHumanPlayers = CountHumanPlayers();

	if (nHumanPlayers > 0)
	{
		return;
	}

	sv_allow_lobby_connect_only->Revert();
}

int CMaxPlayersOverrideExt::GetMaxHumanPlayers()
{
	const int nMaxPlayers = GetMaxPlayersOverrideCmdLineValue();

	if (nMaxPlayers > 0)
	{
		RETURN_META_VALUE(MRES_SUPERCEDE, nMaxPlayers);
	}

	RETURN_META_VALUE(MRES_IGNORED, 0);
}

int CMaxPlayersOverrideExt::GetTotalNumPlayersSupported()
{
	return GetMaxHumanPlayers();
}