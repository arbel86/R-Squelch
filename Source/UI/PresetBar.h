#pragma once
// R-Squelch — preset bar: "<  PRESET NAME  >". Arrows step through every preset; clicking the name opens
// the menu (factory sounds by category, your presets, save, open the presets folder).

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "../Presets.h"

namespace rsq
{
class PresetBar final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit PresetBar (PresetManager& m) : mgr (m)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip ("Presets: click the name for the list and SAVE, arrows to step through them");
        startTimerHz (5);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (Colours::white.withAlpha (0.12f));
        g.fillRoundedRectangle (r.translated (0.0f, 1.0f), 5.0f);
        g.setColour (Colour (0xff0b0c0e));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (theme::gold.withAlpha (0.45f));
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 0.9f);

        auto inner = r.reduced (4.0f, 2.0f);
        auto left = inner.removeFromLeft (16.0f), right = inner.removeFromRight (16.0f);
        for (auto [a, pointsLeft] : { std::pair (left, true), std::pair (right, false) })
        {
            Path p;
            const auto c = a.getCentre();
            if (pointsLeft) p.addTriangle (c.x + 3.0f, c.y - 4.5f, c.x + 3.0f, c.y + 4.5f, c.x - 3.5f, c.y);
            else            p.addTriangle (c.x - 3.0f, c.y - 4.5f, c.x - 3.0f, c.y + 4.5f, c.x + 3.5f, c.y);
            g.setGradientFill (theme::goldGradient (a.reduced (0.0f, 4.0f)));
            g.fillPath (p);
        }
        g.setFont (theme::labelFont (10.5f, true));
        g.setColour (theme::goldHi);
        g.drawText (shownName.toUpperCase(), inner, Justification::centred, true);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.x < 20)                  { mgr.step (-1); refresh(); return; }
        if (e.x > getWidth() - 20)     { mgr.step (+1); refresh(); return; }
        showMenu();
    }

private:
    enum { idInit = 1, idSave, idFolder, idFactory = 100, idUser = 1000 };

    void showMenu()
    {
        juce::PopupMenu menu;
        menu.addItem (idInit, "Init", true, mgr.currentName() == "Init");
        const auto& list = factoryPresets();
        juce::StringArray categories;
        for (auto& p : list) categories.addIfNotAlreadyThere (p.category);
        // every factory sound in one list, grouped under category headings (no hunting through sub-menus)
        for (auto& cat : categories)
        {
            menu.addSectionHeader (cat);
            for (int i = 0; i < (int) list.size(); ++i)
                if (cat == list[(size_t) i].category)
                    menu.addItem (idFactory + i, list[(size_t) i].name, true, mgr.currentName() == list[(size_t) i].name);
        }
        menu.addSeparator();
        const auto user = mgr.userPresetNames();
        juce::PopupMenu mine;
        for (int i = 0; i < user.size(); ++i) mine.addItem (idUser + i, user[i], true, mgr.currentName() == user[i]);
        menu.addSubMenu ("My Presets", mine, user.size() > 0);
        menu.addSeparator();
        menu.addItem (idSave, "Save preset...");
        menu.addItem (idFolder, "Open presets folder");

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
            [safe = juce::Component::SafePointer<PresetBar> (this), user] (int id)
            {
                if (safe == nullptr || id == 0) return;
                auto& m = safe->mgr;
                if (id == idInit) m.loadInit();
                else if (id == idSave) { safe->askName(); return; }
                else if (id == idFolder) { PresetManager::userFolder().createDirectory(); PresetManager::userFolder().startAsProcess(); return; }
                else if (id >= idUser) m.loadUser (user[id - idUser]);
                else if (id >= idFactory) m.loadFactory (id - idFactory);
                safe->refresh();
            });
    }

    void askName()
    {
        auto* w = new juce::AlertWindow ("Save preset", "Name your sound:", juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", mgr.currentName() == "Init" ? juce::String ("My Squelch") : mgr.currentName());
        w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create (
            [safe = juce::Component::SafePointer<PresetBar> (this), w] (int result)
            {
                if (safe != nullptr && result == 1)
                {
                    safe->mgr.saveUser (w->getTextEditorContents ("name"));
                    safe->refresh();
                }
            }), true);
    }

    void refresh() { shownName = mgr.currentName(); repaint(); }
    void timerCallback() override { if (mgr.currentName() != shownName) refresh(); }

    PresetManager& mgr;
    juce::String shownName { "Init" };
};
} // namespace rsq
