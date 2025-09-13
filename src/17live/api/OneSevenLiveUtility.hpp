#pragma once

#include <QString>

// Forward declarations to avoid pulling Qt/Models headers into this header
struct OneSevenLiveRockZoneViewer;
struct OneSevenLiveArmyNameResponse;

// Utility for mapping 17LIVE data to local Qt resource paths or display strings
// All image paths returned are Qt resource URLs like ":/resources/user_images/<file>.png"
class OneSevenLiveUtility {
   public:
    // Avatar frame (circle) over the user's picture based on viewer.type
    // - type = 0: no frame
    // - type = 1: GIFT_RANK_ONE -> ig-firstrank-badge.png
    // - type = 2: GUARDIAN      -> ig-guardian-badge.png
    // - type = 3: ARMY          -> map by armyInfo.rank
    //     rank = 1 SERGEANT -> ig-sergeant-badge.png
    //     rank = 2 CAPTAIN  -> ig-captain-badge.png
    //     rank = 3 COLONEL  -> ig-colonel-badge.png
    //     rank = 4 GENERAL  -> ig-general-badge.png
    //     rank = 5 CORPORAL -> ig-corporal-badge.png
    // Returns empty string if not applicable.
    static QString avatarFrameResource(const OneSevenLiveRockZoneViewer &viewer);

    // mLevel badge based on displayUser.mLevel (0 = none, 1..11 map to local images)
    // Returns empty string if not applicable.
    static QString mLevelBadgeResource(const OneSevenLiveRockZoneViewer &viewer);

    // Open ID display string rule: use displayUser.displayName
    static QString displayOpenID(const OneSevenLiveRockZoneViewer &viewer);

    // Checking level background badge based on displayUser.checkinLevel
    // 0 / 1: none; 2..8 map to ig-bg-checking-*.png
    // Returns empty string if not applicable.
    static QString checkingLevelBadgeResource(const OneSevenLiveRockZoneViewer &viewer);

    // Badge label (text) for a single viewer by type.
    // locale: examples "zh-CN", "zh-TW", "en-US", "ja-JP". Any non-zh/ja falls back to English.
    // For type = 3 (ARMY), if armyResp is provided, it will format:
    //   zh/ja: "Rank %1 (%2)"
    //   en:    "Rank %1 (%2)"
    // If armyResp is null or no matching rank is found, returns empty string for ARMY.
    static QString badgeLabel(int badgeType, int rank,
                              const OneSevenLiveArmyNameResponse *armyResp = nullptr);
};
