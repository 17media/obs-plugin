#include "OneSevenLiveUtility.hpp"

#include <obs-module.h>

#include "OneSevenLiveModels.hpp"

static inline QString res(const char *name) {
    return QString(":/resources/user_images/") + name;
}

QString OneSevenLiveUtility::avatarFrameResource(const OneSevenLiveRockZoneViewer &viewer) {
    switch (viewer.type) {
    case 0:  // NORMAL
        return QString();
    case 1:  // GIFT_RANK_ONE
        return res("ig-firstrank-badge.png");
    case 2:  // GUARDIAN
        return res("ig-guardian-badge.png");
    case 3: {  // ARMY -> by armyInfo.rank
        switch (viewer.armyInfo.rank) {
        case 1:  // SERGEANT
            return res("ig-sergeant-badge.png");
        case 2:  // CAPTAIN
            return res("ig-captain-badge.png");
        case 3:  // COLONEL
            return res("ig-colonel-badge.png");
        case 4:  // GENERAL
            return res("ig-general-badge.png");
        case 5:  // CORPORAL
            return res("ig-corporal-badge.png");
        default:
            return QString();
        }
    }
    default:
        return QString();
    }
}

QString OneSevenLiveUtility::mLevelBadgeResource(const OneSevenLiveRockZoneViewer &viewer) {
    const int lv = viewer.displayUser.mLevel;
    switch (lv) {
    case 0:
        return QString();
    case 1:
        return res("b448cd7f-d366-4b0b-8a98-bba2dd741302.png");
    case 2:
        return res("3dc9b982-d280-46af-82af-9443e646969f.png");
    case 3:
        return res("e716f173-010b-4baa-85c0-8aade65fd80b.png");
    case 4:
        return res("46b95878-34fc-48a5-9c36-d8952b2b2d15.png");
    case 5:
        return res("150a2431-f728-4d8e-8af8-e7c75e3b5c7f.png");
    case 6:
        return res("a84f01d8-04bf-4e7d-b469-6036321e72c0.png");
    case 7:
        return res("acdc5fa1-3be6-4b16-86de-703b6c80dfdc.png");
    case 8:
        return res("4a0dc588-36fb-41bf-ab5e-cd122e114e72.png");
    case 9:
        return res("37bd32cb-f100-4155-9248-84a24d04ccad.png");
    case 10:
        return res("7b211dc9-66b1-43a1-b909-f4d5965e4e7e.png");
    case 11:
        return res("bcc48ea8-606b-4e2d-ac46-230f68c23fd9.png");
    default:
        return QString();
    }
}

QString OneSevenLiveUtility::displayOpenID(const OneSevenLiveRockZoneViewer &viewer) {
    return viewer.displayUser.displayName;
}

QString OneSevenLiveUtility::checkingLevelBadgeResource(const OneSevenLiveRockZoneViewer &viewer) {
    const int lv = viewer.displayUser.checkinLevel;
    switch (lv) {
    case 0:
    case 1:
        return QString();
    case 2:
        return res("ig-bg-checking-green.png");
    case 3:
        return res("ig-bg-checking-blue.png");
    case 4:
        return res("ig-bg-checking-purple.png");
    case 5:
        return res("ig-bg-checking-orange.png");
    case 6:
        return res("ig-bg-checking-silver.png");
    case 7:
        return res("ig-bg-checking-golden.png");
    case 8:
        return res("ig-bg-checking-black.png");
    default:
        return QString();
    }
}

static inline bool isZh(const QString &locale) {
    return locale.startsWith("zh", Qt::CaseInsensitive);
}

static inline bool isJa(const QString &locale) {
    return locale.startsWith("ja", Qt::CaseInsensitive) ||
           locale.startsWith("jp", Qt::CaseInsensitive);
}

QString OneSevenLiveUtility::badgeLabel(int badgeType, int rank,
                                        const OneSevenLiveArmyNameResponse *armyResp) {
    switch (badgeType) {
    case 0:  // NORMAL
        return QString();
    case 1:  // GIFT_RANK_ONE
        return obs_module_text("RockZone.Badge.TopContributor");
    case 2:  // GUARDIAN
        return obs_module_text("RockZone.Badge.Guardian");
    case 3: {  // ARMY -> need rankTier/customName from locale template
        if (!armyResp)
            return QString();
        for (const auto &rn : armyResp->rankName) {
            if (rn.rank == rank) {
                QString text = QString(obs_module_text("RockZone.Badge.Army.Template"))
                                   .arg(QString::number(rn.rankTier), rn.customName);
                return text;
            }
        }
        return QString();
    }
    default:
        return QString();
    }
}
