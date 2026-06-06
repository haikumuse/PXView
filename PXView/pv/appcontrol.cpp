/*
 * This file is part of the PXView project.
 * PXView is based on DSView.
 * PXView is based on PulseView.
 * 
 * Copyright (C) 2021 DreamSourceLab <support@dreamsourcelab.com>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "appcontrol.h"

#include <libsigrok.h>
#include <libsigrokdecode.h>
#include <QDir>
#include <QCoreApplication>
#include <QWidget>
#include <string>
#include <assert.h>
#include "sigsession.h"
#include "dsvdef.h"
#include "config/appconfig.h"
#include "log.h"
#include "utility/path.h"
#include "utility/encoding.h"
#include "data/leaf_block_pool.h"

AppControl::AppControl()
{
    _topWindow = NULL; 
    _session = new pv::SigSession();
}

AppControl::AppControl(AppControl &o)
{
    (void)o;
}
 
AppControl::~AppControl()
{ 
   // DESTROY_OBJECT(_session);
}

AppControl* AppControl::Instance()
{
    static AppControl *ins = NULL;
    if (ins == NULL){
        ins = new AppControl();
    }
    return ins;
}

void AppControl::Destroy(){
    pv::data::LeafBlockPool::instance().drain();
} 

bool AppControl::Init()
{  
    pv::encoding::init();

    QString qs;
    std::string cs;

    qs = GetAppDataDir();
    cs = pv::path::ToUnicodePath(qs);
    pxv_info("GetAppDataDir:\"%s\"", cs.c_str());
    cs = pv::path::ConvertPath(qs);
    ds_set_user_data_dir(cs.c_str());

    qs = GetFirmwareDir();
    cs = pv::path::ToUnicodePath(qs);
    pxv_info("GetFirmwareDir:\"%s\"", cs.c_str());

    qs = GetUserDataDir();
    cs = pv::path::ToUnicodePath(qs);
    pxv_info("GetUserDataDir:\"%s\"", cs.c_str());

    qs = GetDecodeScriptDir();
    cs = pv::path::ToUnicodePath(qs);
    pxv_info("GetDecodeScriptDir:\"%s\"", cs.c_str());
    //---------------end print directorys.

    _session->init();

    srd_log_set_context(pxv_log_context());

#if defined(_WIN32)
    // Set Python home to application directory for embedded Python
    QString pythonHome = QCoreApplication::applicationDirPath();
    QDir pydir(pythonHome);
    QStringList zipFiles = pydir.entryList(QStringList() << "python*.zip", QDir::Files);
    if (!zipFiles.isEmpty()) {
        const wchar_t *pyhome = reinterpret_cast<const wchar_t*>(pythonHome.utf16());
        srd_set_python_home(pyhome);
        pxv_info("Set Python home to: %s", pythonHome.toUtf8().data());
    }
#if defined(DEBUG_INFO)
    //able run debug with qtcreator
    QString pythonHomeDebug = "c:/python";
    QDir pydirDebug;
    if (pydirDebug.exists(pythonHomeDebug)){
        const wchar_t *pyhome = reinterpret_cast<const wchar_t*>(pythonHomeDebug.utf16());
        srd_set_python_home(pyhome);
    }
#endif
#endif
    
    //the python script path of decoder
    char path[256] = {0};
    QString dir = GetDecodeScriptDir();   
    strcpy(path, dir.toUtf8().data());

    // Initialise libsigrokdecode
    if (srd_init(path) != SRD_OK)
    { 
        pxv_err("ERROR: libsigrokdecode init failed.");
        return false;
    }

    // Add C decoder search paths
    {
        QString cDecDir = GetAppDataDir();
        QDir cDecPath(cDecDir);
        if (cDecPath.cd("c_decoders") || cDecPath.cd("../libsigrokdecode/c_decoders")) {
            std::string cs = pv::path::ConvertPath(cDecPath.absolutePath());
            srd_c_decoder_path_add(cs.c_str());
            pxv_info("C decoder path: \"%s\"", cs.c_str());
        }
    }

    // Load the protocol decoders
    if (srd_decoder_load_all() != SRD_OK)
    {
        pxv_err("ERROR: load the protocol decoders failed.");
        return false;
    }
 
    return true;
}

bool AppControl::Start()
{  
    _session->Open(); 
    return true;
}

 void AppControl::Stop()
 {
    _session->Close();  
 }

void AppControl::UnInit()
{  
    // Destroy libsigrokdecode
    srd_exit();

    _session->uninit();
}

bool AppControl::TopWindowIsMaximized()
{
    if (_topWindow != NULL){
        return _topWindow->isMaximized();
    }
    return false;
}