/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include <QDomElement>
#include <QString>
#include <QVector>
#include <unordered_map>

/** @brief This static class provides helper functions to manipulate Dom objects easily
 */
namespace Xml {

/** @brief Set the content of the given @param doc from a file.
 *
 *  It makes sure that the file is open for reading as required by Qt in the future (as of Qt5 2022-10-08)
 *
 *  @param doc
 *  @param fileName
 *  @param namespaceProcessing parameter of QDomDocument::setContent(). If namespaceProcessing is true, the parser recognizes namespaces in the XML file and
 * sets the prefix name, local name and namespace URI to appropriate values. If namespaceProcessing is false, the parser does no namespace processing when it
 * reads the XML file.
 *
 *  @returns false if an error occured while reading or parsing the file to the document
 */
bool docContentFromFile(QDomDocument &doc, const QString &fileName, bool namespaceProcessing);

/** @brief Write the content of the given @param doc from to file.
 *
 *  It makes sure that the file is open for writing
 *
 *  @param doc
 *  @param fileName
 *
 *  @returns false if an error occured while writing the document to the file
 */
bool docContentToFile(const QDomDocument &doc, const QString &fileName);

/** @brief Returns the content of a given tag within the current DomElement.
   For example, if your \@param element looks like <html><title>foo</title><head>bar</head></html>, passing \@tagName = "title" will return foo, and \@tagName
   = "head" returns bar
   Returns empty string if tag is not found.
*/
QString getSubTagContent(const QDomElement &element, const QString &tagName);

/** @brief Returns the direct children of given \@element whose tag name matches given \@param tagName.
   This is an alternative to QDomElement::elementsByTagName which returns also non-direct children
*/
QVector<QDomNode> getDirectChildrenByTagName(const QDomElement &element, const QString &tagName);

/** @brief Returns the content of a children tag of \@param element, which respects the following conditions :
   - Its type is \@param tagName
   - It as an attribute named \@param attribute with value \@param value

   For example, if your element is <html><param val="foo">bar</param></html>, you can retrieve "bar" with parameters: tagName="param", attribute="val", and
   value="foo" Returns \@param defaultReturn when nothing is found. The methods returns the first match found, so make sure there can't be more than one. If
   \@param directChildren is true, only immediate children of the node are considered
*/
QString getTagContentByAttribute(const QDomElement &element, const QString &tagName, const QString &attribute, const QString &value,
                                 const QString &defaultReturn = QString(), bool directChildren = true);

/** @brief This is a specialization of getTagContentByAttribute with tagName = "property" and attribute = "name".
   That is, to match something like <elem><property name="foo">bar</property></elem>, pass propertyName = foo, and this will return bar
*/
QString getXmlProperty(const QDomElement &element, const QString &propertyName, const QString &defaultReturn = QString());
QString getXmlParameter(const QDomElement &element, const QString &propertyName, const QString &defaultReturn = QString());

/** @brief Returns true if the element contains a named property
 */
bool hasXmlProperty(const QDomElement &element, const QString &propertyName);
bool hasXmlParameter(const QDomElement &element, const QString &propertyName);

/** @brief Add properties to the given xml element
   For each element (n, v) in the properties map, it creates a sub element of the form : <property name="n">v</property>
   @param producer is the xml element where to append properties
   @param properties is the map of properties
 */
void addXmlProperties(QDomElement &producer, const std::unordered_map<QString, QString> &properties);
void addXmlProperties(QDomElement &producer, const QMap<QString, QString> &properties);
/** @brief Edit or add a property
 */
void setXmlProperty(QDomElement element, const QString &propertyName, const QString &value);
void setXmlParameter(const QDomElement &element, const QString &propertyName, const QString &value);
/** @brief Remove a property
 */
void removeXmlProperty(QDomElement effect, const QString &name);
void removeMetaProperties(QDomElement producer);

void renameXmlProperty(const QDomElement &effect, const QString &oldName, const QString &newName);

QMap<QString, QString> getXmlPropertyByWildcard(const QDomElement &element, const QString &propertyName);

} // namespace Xml
