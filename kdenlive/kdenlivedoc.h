/***************************************************************************
                          kdenlivedoc.h  -  description
                             -------------------
    begin                : Fri Feb 15 01:46:16 GMT 2002
    copyright            : (C) 2002 by Jason Wood
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

#ifndef KDENLIVEDOC_H
#define KDENLIVEDOC_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif 

// include files for QT
#include <qobject.h>
#include <qstring.h>
#include <qlist.h>

// include files for KDE
#include <kurl.h>

// include files for Kdenlive
#include <avfile.h>
#include <doctrackbase.h>

// forward declaration of the Kdenlive classes
class KdenliveView;

/**	KdenliveDoc provides a document object for a document-view model.
  *
  * The KdenliveDoc class provides a document object that can be used in conjunction with the classes KdenliveApp and KdenliveView
  * to create a document-view model for standard KDE applications based on KApplication and KMainWindow. Thereby, the document object
  * is created by the KdenliveApp instance and contains the document structure with the according methods for manipulation of the document
  * data by KdenliveView objects. Also, KdenliveDoc contains the methods for serialization of the document data from and to files.
  *
  * @author Source Framework Automatically Generated by KDevelop, (c) The KDevelop Team. 	
  * @version KDevelop version 1.2 code generation
  */
class KdenliveDoc : public QObject
{
  Q_OBJECT
  public:
    /** Constructor for the fileclass of the application */
    KdenliveDoc(QWidget *parent, const char *name=0);
    /** Destructor for the fileclass of the application */
    ~KdenliveDoc();

    /** adds a view to the document which represents the document contents. Usually this is your main view. */
    void addView(KdenliveView *view);
    /** removes a view from the list of currently connected views */
    void removeView(KdenliveView *view);
    /** sets the modified flag for the document after a modifying action on the view connected to the document.*/
    void setModified(bool _m=true){ modified=_m; };
    /** returns if the document is modified or not. Use this to determine if your document needs saving by the user on closing.*/
    bool isModified(){ return modified; };
    /** "save modified" - asks the user for saving if the document is modified */
    bool saveModified();	
    /** deletes the document's contents */
    void deleteContents();
    /** initializes the document generally */
    bool newDocument();
    /** closes the acutal document */
    void closeDocument();
    /** loads the document by filename and format and emits the updateViews() signal */
    bool openDocument(const KURL& url, const char *format=0);
    /** saves the document under filename and format.*/	
    bool saveDocument(const KURL& url, const char *format=0);
    /** returns the KURL of the document */
    const KURL& URL() const;
    /** sets the URL of the document */
	  void setURL(const KURL& url);
		/** Returns the internal avFile list. */
		QList<AVFile> avFileList();	
	
  public slots:
    /** calls repaint() on all views connected to the document object and is called by the view by which the document has been changed.
     * As this view normally repaints itself, it is excluded from the paintEvent.
     */
    void slotUpdateAllViews(KdenliveView *sender);
	  /** Inserts an Audio/visual file into the project */
	  void slot_InsertAVFile(const KURL &file);
  	/** Adds a sound track to the project */
  	void addSoundTrack();
  	/** Adds an empty video track to the project */
  	void addVideoTrack();
 	
	 public:	
 		/** the list of the views currently connected to the document */
 		static QList<KdenliveView> *pViewList;	
  	/** The number of frames per second. */
  	int m_framesPerSecond;
  	/** Holds a list of all tracks in the project. */
  	QList<DocTrackBase> m_tracks;
  	/** Returns the number of frames per second. */
  	int framesPerSecond();
  	/** Itterates through the tracks in the project. This works in the same way
			* as QList::next(), although the underlying structures may be different. */
	  DocTrackBase * nextTrack();
  	/** Returns the first track in the project, and resets the itterator to the first track.
			*This effectively is the same as QList::first(), but the underyling implementation
			* may change. */
	  DocTrackBase * firstTrack();
	  /** Returns the number of tracks in this project */
	  int numTracks();

  private:
    /** the modified flag of the current document */
    bool modified;
    KURL doc_url;

		/** List of all video and audio clips within this project */
		QList<AVFile> m_avFileList;		
	signals: // Signals
  	/** This is signal is emitted whenever the avFileList changes, either through the addition or removal of an AVFile, or when an AVFile changes. */
  	void avFileListUpdated(QList<AVFile>);
	private: // Private methods
  	/** Adds a track to the project */
  	void addTrack(DocTrackBase *track);
	signals: // Signals
  	/** This signal is emitted whenever tracks are added to or removed from the project. */
  	void trackListChanged();
};

#endif // KDENLIVEDOC_H
