/*
 * Copyright (C) 2012-2013 AirDC++ Project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef DCPLUSPLUS_WIZARD_AUTOCONN
#define DCPLUSPLUS_WIZARD_AUTOCONN

#include "Async.h"
#include "stdafx.h"
#include "resource.h"
#include "PropPage.h"
#include "Wizard.h"
#include "RichTextBox.h"

#include "../client/ConnectivityManager.h"

#include <atldlgs.h>

class SetupWizard;
class WizardAutoConnectivity : public PropPage, public CAeroWizardPageImpl<WizardAutoConnectivity>, private ConnectivityManagerListener, private Async<WizardAutoConnectivity> { 
public: 
	typedef CAeroWizardPageImpl<WizardAutoConnectivity> baseClass;
	BEGIN_MSG_MAP(WizardAutoConnectivity) 
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog) 
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker) 
		COMMAND_ID_HANDLER(IDC_AUTO_DETECT, OnDetectConnection)
		COMMAND_ID_HANDLER(IDC_IPV4_AUTODETECT, OnTickAutoDetect) 
		COMMAND_ID_HANDLER(IDC_IPV6_AUTODETECT, OnTickAutoDetect) 
		CHAIN_MSG_MAP(baseClass) 
	END_MSG_MAP() 

	LRESULT OnInitDialog(UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /* bHandled */);
	LRESULT OnDetectConnection(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTickAutoDetect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	
	PROPSHEETPAGE *getPSP() { return (PROPSHEETPAGE *)*this; }

	enum { IDD = IDD_WIZARD_AUTOCONN }; 
	WizardAutoConnectivity(SettingsManager *s, SetupWizard* wizard);
	~WizardAutoConnectivity();

	void write();
	int OnWizardNext();
	int OnSetActive();

	bool usingManualConnectivity();
private: 
	enum ProtocolState {
		STATE_UNKNOWN,
		STATE_DETECTING,
		STATE_FAILED,
		STATE_SUCCEED
	};

	ProtocolState v4State;
	ProtocolState v6State;

	SetupWizard* wizard;

	static Item items[];
	static TextItem texts[];

	RichTextBox log;

	CButton cAutoDetect;

	CButton cDetectIPv4;
	CButton cDetectIPv6;

	CButton cManualDetect;

	void addLogLine(tstring& msg, CHARFORMAT2W& cf = WinUtil::m_ChatTextGeneral);
	void detectConnection();

	void updateAuto();
	void changeControlState(bool enable);

	// ConnectivityManagerListener
	void on(Message, const string& message) noexcept;
	void on(Started, bool /*v6*/) noexcept;
	void on(Finished, bool /*v6*/, bool /*failed*/) noexcept;
	void on(SettingChanged) noexcept;
}; 

#endif