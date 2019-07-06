//
// Panel.h
// Vorb Engine
//
// Created by Matthew Marshall 28 June 2019
// Copyright 2019 Regrowth Studios
// MIT License
//

/*! \file Panel.h
* @brief Helpful YAML parsers for Panel.
*/

#pragma once

#ifndef Vorb_YAML_Panel_h__
//! @cond DOXY_SHOW_HEADER_GUARDS
#define Vorb_YAML_Panel_h__
//! @endcond

#include "Vorb/types.h"
#include "Vorb/io/Keg.h"

namespace vorb {
    namespace ui {
        // Forward Declarations.
        class  Panel;
        class  IWidget;

        /*!
         * \brief Parses the entry of the currently-parsing Panel corresponding to the given YAML node.
         *
         * \param context The YAML context to use for reading nodes.
         * \param Panel The Panel to be built up using the given parsed Panel data.
         * \param name The name of the Panel property to be parsed.
         * \param node The node of the Panel property to be parsed.
         * \param widgetParser Marshals parsing of the widgets that are children of the Panel being parsed.
         *
         * \return True if parsing is successful, false otherwise.
         */
        bool parsePanelEntry(keg::ReadContext& context, vui::Panel* checkBox, const nString& name, keg::Node value, Delegate<vui::IWidget*, const nString&, keg::Node>* widgetParser, vg::TextureCache* cache);
    }
}
namespace vui = vorb::ui;

#endif // !Vorb_YAML_Panel_h__