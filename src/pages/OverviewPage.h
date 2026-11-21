#pragma once

#include "app/Page.h"
#include "app/Widgets.h"
#include "core/InjectionSequence.h"
#include "core/ProxyMonitor.h"

class AppButton;
class Card;
class IconTile;
class StyledLabel;

/*
 * 总览: hero status card, live stat cards (A client / listener / B client),
 * quick-start guide and notes.
 */
class OverviewPage : public Page
{
    Q_OBJECT

public:
    explicit OverviewPage(QWidget* parent = nullptr);

    void updateSnapshot(const ProxySnapshot& snapshot, InjectionSequence::State state);

signals:
    void startRequested();
    void viewLogsRequested();

private:
    void buildStatCards();
    void buildGuideCard();
    void buildNotesCard();

    // Hero.
    Card* m_hero = nullptr;
    IconTile* m_heroIcon = nullptr;
    StyledLabel* m_heroTitle = nullptr;
    StyledLabel* m_heroSub = nullptr;
    AppButton* m_ctaPrimary = nullptr;
    AppButton* m_ctaSecondary = nullptr;

    // Stat cards.
    StyledLabel* m_aValue = nullptr;
    StyledLabel* m_aCaption = nullptr;
    StyledLabel* m_listenerValue = nullptr;
    StyledLabel* m_listenerCaption = nullptr;
    StyledLabel* m_bValue = nullptr;
    StyledLabel* m_bCaption = nullptr;

    ProxySnapshot m_snapshot;
    int m_seqState = 0; // InjectionSequence::State
};
