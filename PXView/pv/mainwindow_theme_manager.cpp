/*
 * This file is part of the PXView project.
 * PXView is based on DSView.
 * PXView is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2013 DreamSourceLab <support@dreamsourcelab.com>
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

#include "mainwindow_theme_manager.h"
#include "mainwindow.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTranslator>

#include "config/appconfig.h"
#include "pxvdef.h"
#include "toolbars/filebar.h"
#include "toolbars/logobar.h"
#include "toolbars/titlebar.h"
#include "toolbars/trigbar.h"
#include "ui/langresource.h"
#include "ui/uimanager.h"
#include "sigsession.h"
#include "view/view.h"
#include "view/viewstatus.h"

namespace pv {

MainWindowThemeManager::MainWindowThemeManager(MainWindow *wnd)
    : _wnd(wnd) {}

void MainWindowThemeManager::switchTheme(QString style) {
  AppConfig &app = AppConfig::Instance();

  if (app.frameOptions.style != style) {
    app.frameOptions.style = style;
    app.SaveFrame();
  }

  QString qssRes = ":/theme.qss";
  QFile qss(qssRes);
  if (!qss.open(QFile::ReadOnly | QFile::Text)) {
    return;
  }
  QString qssContent = qss.readAll();
  qss.close();

  QHash<QString, QString> tokens;

  // Load base tokens from JSON schema instance
  QString jsonRes = ":/" + style + ".json";
  QFile jsonFile(jsonRes);
  if (jsonFile.open(QFile::ReadOnly | QFile::Text)) {
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonFile.readAll());
    QJsonObject rootObj = jsonDoc.object();
    QJsonObject tokensObj = rootObj.value("tokens").toObject();
    for (const QString &key : tokensObj.keys()) {
      tokens[key] = tokensObj.value(key).toString();
    }
    jsonFile.close();
  } else {
    // Fallback: parse from QSS if JSON is missing
    QRegularExpression tokenRe(
        "@([\\w-]+):\\s*([^\\r\\n]+?)\\s*(?:\\*/|\\r|\\n)");
    QRegularExpressionMatchIterator it = tokenRe.globalMatch(qssContent);
    while (it.hasNext()) {
      QRegularExpressionMatch match = it.next();
      QString tokenName = "@" + match.captured(1);
      QString tokenValue = match.captured(2).trimmed();
      tokens[tokenName] = tokenValue;
    }
  }

  for (int i = 0; i < app.styleOptions.items.size(); i++) {
    tokens[app.styleOptions.items[i].tokenName] =
        app.styleOptions.items[i].value;
  }

  QList<QString> keys = tokens.keys();
  std::sort(keys.begin(), keys.end(), [](const QString &a, const QString &b) {
    return a.length() > b.length();
  });

  for (const QString &key : keys) {
    qssContent.replace(key, tokens[key]);
  }

  // Process SVG files that contain token placeholders (e.g. @accent)
  QString tempDir =
      QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
      "/pxview_themed_svgs";
  QDir().mkpath(tempDir);

  QRegularExpression svgRe("image:\\s*url\\((:[^)]+\\.svg)\\)");
  QRegularExpressionMatchIterator svgIt = svgRe.globalMatch(qssContent);
  QSet<QString> processedSvgs;
  while (svgIt.hasNext()) {
    QRegularExpressionMatch match = svgIt.next();
    QString svgResPath = match.captured(1);

    if (processedSvgs.contains(svgResPath))
      continue;
    processedSvgs.insert(svgResPath);

    QFile svgFile(svgResPath);
    if (!svgFile.open(QFile::ReadOnly | QFile::Text))
      continue;
    QString svgContent = svgFile.readAll();
    svgFile.close();

    bool hasPlaceholders = false;
    for (const QString &key : keys) {
      if (svgContent.contains(key)) {
        hasPlaceholders = true;
        break;
      }
    }
    if (!hasPlaceholders)
      continue;

    for (const QString &key : keys) {
      svgContent.replace(key, tokens[key]);
    }

    QString fileName = svgResPath;
    fileName.replace(":/", "");
    fileName.replace("/", "_");
    QString tempPath = tempDir + "/" + fileName;
    QFile tempFile(tempPath);
    if (tempFile.open(QFile::WriteOnly | QFile::Text)) {
      tempFile.write(svgContent.toUtf8());
      tempFile.close();
    }

    qssContent.replace(svgResPath, tempPath);
  }

  app.SetThemeTokens(tokens);

  qApp->setStyleSheet(qssContent);

  UiManager::Instance()->Update(UI_UPDATE_ACTION_THEME);
  UiManager::Instance()->Update(UI_UPDATE_ACTION_FONT);

  _wnd->data_updated();
  retranslateRibbon();
}

void MainWindowThemeManager::switchLanguage(int language) {
  if (language == 0)
    return;

  AppConfig &app = AppConfig::Instance();

  if (app.frameOptions.language != language && language > 0) {
    app.frameOptions.language = language;
    app.SaveFrame();
    LangResource::Instance()->Load(language);
  }

  if (language == LAN_CN) {
    (void)_wnd->_qtTrans.load(":/qt_" + QString::number(language));
    qApp->installTranslator(&_wnd->_qtTrans);
    (void)_wnd->_myTrans.load(":/my_" + QString::number(language));
    qApp->installTranslator(&_wnd->_myTrans);
  } else if (language == LAN_EN) {
    qApp->removeTranslator(&_wnd->_qtTrans);
    qApp->removeTranslator(&_wnd->_myTrans);
  }

  _wnd->retranslateUi();

  UiManager::Instance()->Update(UI_UPDATE_ACTION_LANG);
  _wnd->_session->update_lang_text();
}

void MainWindowThemeManager::setupRibbonCategories() {
  // Add Ribbon categories
  _wnd->_category_file_index = _wnd->_title_bar->addCategory(
      L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_FILE), "File"));
  _wnd->_category_display_index = _wnd->_title_bar->addCategory(
      L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_DISPLAY), "Settings"));
  _wnd->_category_help_index = _wnd->_title_bar->addCategory(
      L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_HELP), "Help"));

  // File category
  _wnd->_title_bar->addAction(_wnd->_category_file_index, _wnd->_file_bar->_action_load);
  _wnd->_title_bar->addAction(_wnd->_category_file_index, _wnd->_file_bar->_action_store);
  _wnd->_title_bar->addAction(_wnd->_category_file_index, _wnd->_file_bar->_action_default);
  _wnd->_title_bar->addSeparator(_wnd->_category_file_index);
  _wnd->_title_bar->addAction(_wnd->_category_file_index, _wnd->_file_bar->_action_open);
  _wnd->_title_bar->addAction(_wnd->_category_file_index, _wnd->_file_bar->_action_save);
  _wnd->_title_bar->addSeparator(_wnd->_category_file_index);
  _wnd->_title_bar->addAction(_wnd->_category_file_index, _wnd->_file_bar->_action_export);
  _wnd->_title_bar->addAction(_wnd->_category_file_index, _wnd->_file_bar->_action_import);
  _wnd->_title_bar->addAction(_wnd->_category_file_index, _wnd->_file_bar->_action_capture);

  // Display category
  _wnd->_title_bar->addAction(_wnd->_category_display_index, _wnd->_logo_bar->_action_cn);
  _wnd->_title_bar->addAction(_wnd->_category_display_index, _wnd->_logo_bar->_action_traditional);
  _wnd->_title_bar->addAction(_wnd->_category_display_index, _wnd->_logo_bar->_action_en);
  _wnd->_title_bar->addSeparator(_wnd->_category_display_index);
  _wnd->_title_bar->addAction(_wnd->_category_display_index, _wnd->_trig_bar->_action_dispalyOptions);

  // Help category
  _wnd->_title_bar->addAction(_wnd->_category_help_index, _wnd->_logo_bar->_about);
  _wnd->_title_bar->addAction(_wnd->_category_help_index, _wnd->_logo_bar->_manual);
  _wnd->_title_bar->addAction(_wnd->_category_help_index, _wnd->_logo_bar->_issue);
  _wnd->_title_bar->addAction(_wnd->_category_help_index, _wnd->_logo_bar->_update);
}

void MainWindowThemeManager::retranslateRibbon() {
  if (_wnd->_title_bar) {
    _wnd->_title_bar->retranslateUi(
        _wnd->_category_file_index,
        L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_FILE), "File"));
    _wnd->_title_bar->retranslateUi(
        _wnd->_category_display_index,
        L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_DISPLAY), "Settings"));
    _wnd->_title_bar->retranslateUi(
        _wnd->_category_help_index,
        L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_HELP), "Help"));
  }
}

} // namespace pv
