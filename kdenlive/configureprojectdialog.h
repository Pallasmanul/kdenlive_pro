/***************************************************************************
                          configureprojectdialog  -  description
                             -------------------
    begin                : Sat Nov 15 2003
    copyright            : (C) 2003 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef CONFIGUREPROJECTDIALOG_H
#define CONFIGUREPROJECTDIALOG_H

#include <kdialogbase.h>
#include <kurl.h>

// #include "avfileformatdesc.h"
#include "kdenlive.h"
#include "configureproject_ui.h"

class QSplitter;
class QVBox;
class KJanusWidget;
class KListBox;
class KPushButton;

class ExportConfig;
class ConfigureProject;


class ProjectTemplate
{
    public:
        ProjectTemplate() : m_name( QString::null ) { }
        ProjectTemplate( const QString& name, double fps, int width, int height )
    : m_name( name), m_fps( fps ), m_width( width ), m_height( height )
        { }

        QString name() const { return m_name; }
        double fps() const { return m_fps; }
        int width() const { return m_width; }
        int height() const { return m_height; }

    private:
        QString m_name;
        double m_fps;
        int m_width;
        int m_height;
};



/**
Dialog for configuring the project. This configuration dialog is used for both project creation and simply changing the project configuration

@author Jason Wood
*/
class ConfigureProjectDialog:public ConfigureProjectPanel_UI {
  Q_OBJECT public:
    ConfigureProjectDialog(Gui::KdenliveApp * = 0, const char *name = 0, WFlags f = 0);
    ~ConfigureProjectDialog();

    QString selectedFormat();
    QString selectedFolder();
    void loadTemplates();

    public slots:		// Public slots
    void updateDisplay();
    void slotEditMetadata();

  private:
    Gui::KdenliveApp *m_app;
    ConfigureProject *m_config;
};

#endif
