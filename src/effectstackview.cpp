/***************************************************************************
                          effecstackview.cpp  -  description
                             -------------------
    begin                : Feb 15 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <KDebug>
#include <KLocale>

#include "effectstackview.h"
#include "effectslist.h"
#include "clipitem.h"
#include <QHeaderView>
#include <QMenu>

EffectStackView::EffectStackView(EffectsList *audioEffectList, EffectsList *videoEffectList, EffectsList *customEffectList, QWidget *parent)
: QWidget(parent)
{
	ui.setupUi(this);
	effectedit=new EffectStackEdit(ui.frame,this);
	//ui.effectlist->horizontalHeader()->setVisible(false);
	//ui.effectlist->verticalHeader()->setVisible(false);
	clipref=NULL;
	
	ui.buttonNew->setIcon(KIcon("document-new"));
	ui.buttonNew->setToolTip(i18n("Add new effect"));
	ui.buttonUp->setIcon(KIcon("go-up"));
	ui.buttonUp->setToolTip(i18n("Move effect up"));
	ui.buttonDown->setIcon(KIcon("go-down"));
	ui.buttonDown->setToolTip(i18n("Move effect down"));
	ui.buttonDel->setIcon(KIcon("trash-empty"));
	ui.buttonDel->setToolTip(i18n("Delete effect"));
	

	
	ui.effectlist->setDragDropMode(QAbstractItemView::NoDragDrop);//use internal if drop is recognised right
	
	connect (ui.effectlist, SIGNAL ( itemSelectionChanged()), this , SLOT( slotItemSelectionChanged() ));
	connect (ui.buttonNew, SIGNAL (clicked()), this, SLOT (slotNewEffect()) );
	connect (ui.buttonUp, SIGNAL (clicked()), this, SLOT (slotItemUp()) );
	connect (ui.buttonDown, SIGNAL (clicked()), this, SLOT (slotItemDown()) );
	connect (ui.buttonDel, SIGNAL (clicked()), this, SLOT (slotItemDel()) );
	connect( this, SIGNAL (transferParamDesc(const QDomElement&,int ,int) ), effectedit , SLOT(transferParamDesc(const QDomElement&,int ,int)));
	connect(effectedit, SIGNAL (parameterChanged(const QDomElement&  ) ), this , SLOT (slotUpdateEffectParams(const QDomElement&)));
	effectLists["audio"]=audioEffectList;
	effectLists["video"]=videoEffectList;
	effectLists["custom"]=customEffectList;
	
	ui.infoBox->hide();	
	setEnabled(false);
	
}

void EffectStackView::slotUpdateEffectParams(const QDomElement& e){
	//effects[ui.effectlist->currentRow()]=e;
	if (clipref)
		emit updateClipEffect(clipref, e);
}

void EffectStackView::slotClipItemSelected(ClipItem* c)
{
	clipref=c;
	if (clipref==NULL) {
		setEnabled(false);
		return;
	}
	setEnabled(true);
	//effects=clipref->effectNames();
	for (int i=0;i<clipref->effectsCount();i++){
		QDomElement element=clipref->effectAt(i);//kDebug()<<"// SET STACK :"<<element.attribute("kdenlive_ix")<<", ("<<i<<") = "<<element.attribute("tag");
	}
	setupListView();
	
}

void EffectStackView::setupListView(){

	ui.effectlist->clear();
	for (int i=0;i<clipref->effectsCount();i++){
		QDomElement d=clipref->effectAt(i);
		QDomNode namenode = d.elementsByTagName("name").item(0);
		if (!namenode.isNull()) {
			QListWidgetItem* item = new QListWidgetItem(namenode.toElement().text(), ui.effectlist);
			item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsDragEnabled|Qt::ItemIsUserCheckable|Qt::ItemIsEnabled);
			item->setCheckState(Qt::Checked);
		}
	}
	ui.effectlist->setCurrentRow(0);
}

void EffectStackView::slotItemSelectionChanged(){
	bool hasItem = ui.effectlist->currentItem();
	int activeRow = ui.effectlist->currentRow();
	if (hasItem && ui.effectlist->currentItem()->isSelected() ){
		emit transferParamDesc(clipref->effectAt(activeRow) ,0,100);//minx max frame
	}
	ui.buttonDel->setEnabled( hasItem );
	ui.buttonUp->setEnabled( activeRow >0 );
	ui.buttonDown->setEnabled( (activeRow < ui.effectlist->count()-1) && hasItem );
}

void EffectStackView::slotItemUp(){
	int activeRow = ui.effectlist->currentRow();
	if (activeRow>0){
		QDomElement act = clipref->effectAt(activeRow).cloneNode().toElement();
		QDomElement before = clipref->effectAt(activeRow-1).cloneNode().toElement();
		clipref->setEffectAt(activeRow-1, act);
		clipref->setEffectAt(activeRow, before);
		//renumberEffects();
		//effects.swap(activeRow, activeRow-1);
	}
	QListWidgetItem *item = ui.effectlist->takeItem(activeRow);
	ui.effectlist->insertItem (activeRow-1, item);
	ui.effectlist->setCurrentItem(item);
	emit refreshEffectStack(clipref);
}

void EffectStackView::slotItemDown(){
	int activeRow = ui.effectlist->currentRow();
	if (activeRow < ui.effectlist->count()-1){
		QDomElement act = clipref->effectAt(activeRow).cloneNode().toElement();
		QDomElement after = clipref->effectAt(activeRow+1).cloneNode().toElement();
		clipref->setEffectAt(activeRow+1, act);
		clipref->setEffectAt(activeRow, after);
		//renumberEffects();
		//effects.swap(activeRow, activeRow+1);
	}
	QListWidgetItem *item = ui.effectlist->takeItem(activeRow);
	ui.effectlist->insertItem (activeRow+1, item);
	ui.effectlist->setCurrentItem(item);
	emit refreshEffectStack(clipref);
}

void EffectStackView::slotItemDel(){
	int activeRow = ui.effectlist->currentRow();
	if ( activeRow>=0  ){
		emit removeEffect(clipref, clipref->effectAt(activeRow));
		//effects.take(activeRow);
		//renumberEffects();
		//effects.removeAt(activeRow);
	}
	/*if (effects.size()>0 && activeRow>0) {
		QListWidgetItem *item = ui.effectlist->takeItem(activeRow);
		kDebug()<<"777777   DELETING ITEM: "<<activeRow;
		delete item;
	}*/
	
}

void EffectStackView::renumberEffects(){/*
	QMap<int,QDomElement> tmplist=effects;
	QMapIterator<int,QDomElement> it(tmplist);
	effects.clear();
	int i=0;
	
	while (it.hasNext()){
		it.next();
		QDomElement item=it.value();
		int currentVal = item.attributes().namedItem("kdenlive_ix").nodeValue().toInt();
		item.attributes().namedItem("kdenlive_ix").setNodeValue(QString::number(i));
		effects[i]=item;
		if (clipref && i != currentVal)
			emit updateClipEffect(clipref,item);
		QString outstr;
		QTextStream str(&outstr);
		item.save(str,2);
		kDebug() << "nummer: " << i << " " << outstr;
		kDebug()<<"EFFECT "<<i<<" = "<<item.attribute("tag");
		i++;
	}*/
	
}

void EffectStackView::slotNewEffect(){
	

	QMenu *displayMenu=new QMenu (this);
	displayMenu->setTitle("Filters");
	foreach (QString type, effectLists.keys() ){
		QAction *a=new QAction(type,displayMenu);
		EffectsList *list=effectLists[type];

		QMenu *parts=new QMenu(type,displayMenu);
		parts->setTitle(type);
		foreach (QString name, list->effectNames()){
			QAction *entry=new QAction(name,parts);
			entry->setData(name);
			entry->setToolTip(list->getInfo(name));
			entry->setStatusTip(list->getInfo(name));
			parts->addAction(entry);
			//QAction
		}
		displayMenu->addMenu(parts);

	}

	QAction *result=displayMenu->exec(mapToGlobal(ui.buttonNew->pos()+ui.buttonNew->rect().bottomRight()));
	
	if (result){
		//TODO effects.append(result->data().toString());
		foreach (EffectsList* e, effectLists.values()){
			QDomElement dom=e->getEffectByName(result->data().toString());
			if (clipref)
				clipref->addEffect(dom);
			slotClipItemSelected(clipref);
		}
		
		setupListView();
		//kDebug()<< result->data();
	}
	delete displayMenu;
	
}

void EffectStackView::itemSelectionChanged (){
	//kDebug() << "drop";
}
#include "effectstackview.moc"
