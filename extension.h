#pragma once

#include "smsdk_ext.h"
#include <IBinTools.h>
#include <ISDKTools.h>
#include <iplayerinfo.h>

class CMaxPlayersOverrideExt : public SDKExtension, public IClientListener
{
public:
	/**
	 * @brief This is called after the initial loading sequence has been processed.
	 *
	 * @param error		Error message buffer.
	 * @param maxlen	Size of error message buffer.
	 * @param late		Whether or not the module was loaded after map load.
	 * @return			True to succeed loading, false to fail.
	 */
	virtual bool SDK_OnLoad(char *error, size_t maxlen, bool late) override;

	/**
	 * @brief This is called right before the extension is unloaded.
	 */
	virtual void SDK_OnUnload() override;

	/**
	 * @brief This is called once all known extensions have been loaded.
	 * Note: It is is a good idea to add natives here, if any are provided.
	 */
	virtual void SDK_OnAllLoaded() override;

#if defined SMEXT_CONF_METAMOD
	/**
	 * @brief Called when Metamod is attached, before the extension version is called.
	 *
	 * @param error			Error buffer.
	 * @param maxlen		Maximum size of error buffer.
	 * @param late			Whether or not Metamod considers this a late load.
	 * @return				True to succeed, false to fail.
	 */
	virtual bool SDK_OnMetamodLoad(ISmmAPI *ismm, char *error, size_t maxlen, bool late) override;

	/**
	 * @brief Called when Metamod is detaching, after the extension version is called.
	 * NOTE: By default this is blocked unless sent from SourceMod.
	 *
	 * @param error			Error buffer.
	 * @param maxlen		Maximum size of error buffer.
	 * @return				True to succeed, false to fail.
	 */
	// virtual bool SDK_OnMetamodUnload(char *error, size_t maxlength) override;
#endif

	/**
	 * @brief Return false to tell Core that your extension should be considered unusable.
	 *
	 * @param error				Error buffer.
	 * @param maxlength			Size of error buffer.
	 * @return					True on success, false otherwise.
	 */
	virtual bool QueryRunning(char *error, size_t maxlength) override;

	/**
	 * @brief Called when a client has connected.
	 *
	 * @param client		Index of the client.
	 */
	virtual void OnClientConnected(int client) override;

	/**
	 * @brief Called when the server is activated.
	 */
	virtual void OnServerActivated(int max_clients) override;

	/**
	 * @brief Called on server activation before plugins receive the OnServerLoad forward.
	 *
	 * @param pEdictList		Edicts list.
	 * @param edictCount		Number of edicts in the list.
	 * @param clientMax			Maximum number of clients allowed in the server.
	 */
	virtual void OnCoreMapStart(edict_t *pEdictList, int edictCount, int clientMax) override;

	/**
	 * @brief Called on level shutdown
	 *
	 */
	virtual void OnCoreMapEnd() override;

private:
	void *m_pfn_CBaseServer_SetReservationCookie = nullptr;

	IBinTools *m_pBinTools = nullptr;
	ISDKTools *m_pSDKTools = nullptr;

	bool m_bLate = false;

	int m_iMaxPlayers = 0;

	int m_nOffset_ConVar_m_fnChangeCallback = -1;

	int m_nSHookID_CGameRules_GetMaxHumanPlayers = -1;
	int m_nSHookID_CMultiplayRules_ClientDisconnected = -1;
	int m_nSHookID_CMatchTitle_GetTotalNumPlayersSupported = -1;

	int GetMaxPlayersOverrideCmdLineValue() const;
	int CountHumanPlayers() const;

	void SetupGameRulesHooks();
	void CleanupGameRulesHooks();
	void SetReservationCookie(uint64_t ullCookie, char const *pszReasonFormat, ...) const;
	void TryClearReservationCookie() const;

	// Called to apply lobby settings to a dedicated server
	void ApplyGameSettings(KeyValues *pRequest);

	void ServerHibernationUpdate(bool bHibernating);
	void ClientDisconnected(edict_t *pClient);

	// Return # of human slots, -1 if can't determine or don't care (engine will assume it's == maxplayers )
	int GetMaxHumanPlayers();

	// Get total number of players supported by the title
	int GetTotalNumPlayersSupported();
};