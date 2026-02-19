#include "stdafx.h"

#include <Defines.h>
#include <Logger.h>
#include <ctime>

#include <GWCA/GameEntities/Item.h>

#include <GWCA/Managers/ItemMgr.h>

#include <GWCA/Constants/Constants.h>

#include <GWCA/Utilities/Hook.h>

#include <Modules/GameSettings.h>
#include <Modules/ItemDescriptionHandler.h>
#include <Modules/Resources.h>
#include <Modules/PriceCheckerModule.h>

#include <Timer.h>
#include <Utils/GuiUtils.h>
#include <Constants/EncStrings.h>
#include <Utils/TextUtils.h>

using nlohmann::json;

namespace {
    float high_price_threshold = 1000;
    bool fetching_prices;
    const char* trader_quotes_url = "https://kamadan.gwtoolbox.com/trader_quotes";

    constexpr clock_t request_interval = CLOCKS_PER_SEC * 60 * 5;
    clock_t last_request_time = request_interval * -1;
    std::unordered_map<std::string, uint32_t> prices_by_identifier;

    size_t generate_item_identifier(const GW::Item* item, std::string& out)
    {
        if (!item) {
            out.resize(0);
            return 0;
        }
        size_t write_offset = 0;
        out.resize(32);
        size_t length = out.size();

        char* data = out.data();

        write_offset += snprintf(data, length, "%02x%04x", (uint8_t)item->type, (uint16_t)item->model_id);

        const uint16_t* mod_filter = NULL;
        size_t mod_filter_len = 0;

        static const uint16_t rune_mods[] = {0x21e, 0x27e, 0xa53, 0x253};
        static const uint16_t dye_mods[] = {0x24d};

        switch (item->type) {
            case GW::Constants::ItemType::Rune_Mod:
                mod_filter = rune_mods;
                mod_filter_len = _countof(rune_mods);
                break;
            case GW::Constants::ItemType::Dye:
                mod_filter = dye_mods;
                mod_filter_len = _countof(dye_mods);
                break;
            default:
                break;
        }
        for (size_t j = 0; j < mod_filter_len; j++) {
            for (size_t i = 0; i < item->mod_struct_size && write_offset < length; i++) {
                uint16_t mod_struct_identifier = (item->mod_struct[i].mod >> 0x14) & 0x3ff;
                if (mod_filter[j] == mod_struct_identifier) {
                    // Write the first mod identifier match we find; this should be enough to identify the item
                    write_offset += snprintf(&data[write_offset], length - write_offset, "%08x", item->mod_struct[i].mod);
                    out.resize(write_offset);
                    return write_offset;
                }
            }
        }

        out.resize(write_offset);
        return write_offset;
    }

    bool ParsePriceJson(const std::string& prices_json_str) {

        const json& prices_json = json::parse(prices_json_str, nullptr, false);
        if (prices_json == json::value_t::discarded) {
            return false;
        }

        prices_by_identifier.clear();
        const auto& it_buy = prices_json.find("sell");
        if (it_buy == prices_json.end() || !it_buy->is_object()) {
            return false;
        }

        const auto& buy = it_buy.value();

        for (auto it = buy.begin(); it != buy.end(); it++) {
            const auto& identifier = it.key();
            if (!it->is_object())
                continue;
            const auto& price_value = it->find("p");
            if (!(price_value != it->end() && price_value->is_number_unsigned()))
                continue;
            prices_by_identifier[identifier] = price_value->get<uint32_t>();
        }
        return !prices_by_identifier.empty();
    }
    struct PriceInfo {
        const char* name;
        const char* id;
    };

    static const std::unordered_map<uint32_t, PriceInfo> price_info_by_unique_mod_struct = {
        {0x25300423, {"Rune of Attunement", "08038225300423"}},
        {0x25300425, {"Rune of Vitae", "08038225300425"}},
        {0x27e802c2, {"Rune of Minor Vigor", "08038227e802c2"}},
        {0x21e80001, {"Mesmer Rune of Minor Fast Casting", "08038321e80001"}},
        {0x21e80101, {"Mesmer Rune of Minor Illusion Magic", "08038321e80101"}},
        {0x21e80201, {"Mesmer Rune of Minor Domination Magic", "08038321e80201"}},
        {0x21e80301, {"Mesmer Rune of Minor Inspiration Magic", "08038321e80301"}},
        {0x21e80401, {"Necromancer Rune of Minor Blood Magic", "08038421e80401"}},
        {0x21e80501, {"Necromancer Rune of Minor Death Magic", "08038421e80501"}},
        {0x21e80601, {"Necromancer Rune of Minor Soul Reaping", "08038421e80601"}},
        {0x21e80701, {"Necromancer Rune of Minor Curses", "08038421e80701"}},
        {0x21e80801, {"Elementalist Rune of Minor Air Magic", "08038521e80801"}},
        {0x21e80901, {"Elementalist Rune of Minor Earth Magic", "08038521e80901"}},
        {0x21e80a01, {"Elementalist Rune of Minor Fire Magic", "08038521e80a01"}},
        {0x21e80b01, {"Elementalist Rune of Minor Water Magic", "08038521e80b01"}},
        {0x21e80c01, {"Elementalist Rune of Minor Energy Storage", "08038521e80c01"}},
        {0x21e80d01, {"Monk Rune of Minor Healing Prayers", "08038621e80d01"}},
        {0x21e80e01, {"Monk Rune of Minor Smiting Prayers", "08038621e80e01"}},
        {0x21e80f01, {"Monk Rune of Minor Protection Prayers", "08038621e80f01"}},
        {0x21e81001, {"Monk Rune of Minor Divine Favor", "08038621e81001"}},
        {0x21e81101, {"Warrior Rune of Minor Strength", "08038721e81101"}},
        {0x21e81201, {"Warrior Rune of Minor Axe Mastery", "08038721e81201"}},
        {0x21e81301, {"Warrior Rune of Minor Hammer Mastery", "08038721e81301"}},
        {0x21e81401, {"Warrior Rune of Minor Swordsmanship", "08038721e81401"}},
        {0x21e81501, {"Warrior Rune of Minor Tactics", "08038721e81501"}},
        {0x27e802ea, {"Warrior Rune of Minor Absorption", "08038727e802ea"}},
        {0x21e81601, {"Ranger Rune of Minor Beast Mastery", "08038821e81601"}},
        {0x21e81701, {"Ranger Rune of Minor Expertise", "08038821e81701"}},
        {0x21e81801, {"Ranger Rune of Minor Wilderness Survival", "08038821e81801"}},
        {0x21e81901, {"Ranger Rune of Minor Marksmanship", "08038821e81901"}},
        {0x21e80002, {"Mesmer Rune of Major Fast Casting", "080e1c21e80002"}},
        {0x21e80102, {"Mesmer Rune of Major Illusion Magic", "080e1c21e80102"}},
        {0x21e80202, {"Mesmer Rune of Major Domination Magic", "080e1c21e80202"}},
        {0x21e80302, {"Mesmer Rune of Major Inspiration Magic", "080e1c21e80302"}},
        {0x21e80003, {"Mesmer Rune of Superior Fast Casting", "0815ad21e80003"}},
        {0x21e80103, {"Mesmer Rune of Superior Illusion Magic", "0815ad21e80103"}},
        {0x21e80203, {"Mesmer Rune of Superior Domination Magic", "0815ad21e80203"}},
        {0x21e80303, {"Mesmer Rune of Superior Inspiration Magic", "0815ad21e80303"}},
        {0x25300427, {"Rune of Recovery", "0815ae25300427"}},
        {0x25300429, {"Rune of Restoration", "0815ae25300429"}},
        {0x2530042b, {"Rune of Clarity", "0815ae2530042b"}},
        {0x2530042d, {"Rune of Purity", "0815ae2530042d"}},
        {0x27e902c2, {"Rune of Major Vigor", "0815ae27e902c2"}},
        {0x27ea02c2, {"Rune of Superior Vigor", "0815af27ea02c2"}},
        {0x21e80402, {"Necromancer Rune of Major Blood Magic", "0815b021e80402"}},
        {0x21e80502, {"Necromancer Rune of Major Death Magic", "0815b021e80502"}},
        {0x21e80602, {"Necromancer Rune of Major Soul Reaping", "0815b021e80602"}},
        {0x21e80702, {"Necromancer Rune of Major Curses", "0815b021e80702"}},
        {0x21e80403, {"Necromancer Rune of Superior Blood Magic", "0815b121e80403"}},
        {0x21e80503, {"Necromancer Rune of Superior Death Magic", "0815b121e80503"}},
        {0x21e80603, {"Necromancer Rune of Superior Soul Reaping", "0815b121e80603"}},
        {0x21e80703, {"Necromancer Rune of Superior Curses", "0815b121e80703"}},
        {0x21e80802, {"Elementalist Rune of Major Air Magic", "0815b221e80802"}},
        {0x21e80902, {"Elementalist Rune of Major Earth Magic", "0815b221e80902"}},
        {0x21e80a02, {"Elementalist Rune of Major Fire Magic", "0815b221e80a02"}},
        {0x21e80b02, {"Elementalist Rune of Major Water Magic", "0815b221e80b02"}},
        {0x21e80c02, {"Elementalist Rune of Major Energy Storage", "0815b221e80c02"}},
        {0x21e80803, {"Elementalist Rune of Superior Air Magic", "0815b321e80803"}},
        {0x21e80903, {"Elementalist Rune of Superior Earth Magic", "0815b321e80903"}},
        {0x21e80a03, {"Elementalist Rune of Superior Fire Magic", "0815b321e80a03"}},
        {0x21e80b03, {"Elementalist Rune of Superior Water Magic", "0815b321e80b03"}},
        {0x21e80c03, {"Elementalist Rune of Superior Energy Storage", "0815b321e80c03"}},
        {0x21e80d02, {"Monk Rune of Major Healing Prayers", "0815b421e80d02"}},
        {0x21e80e02, {"Monk Rune of Major Smiting Prayers", "0815b421e80e02"}},
        {0x21e80f02, {"Monk Rune of Major Protection Prayers", "0815b421e80f02"}},
        {0x21e81002, {"Monk Rune of Major Divine Favor", "0815b421e81002"}},
        {0x21e80d03, {"Monk Rune of Superior Healing Prayers", "0815b521e80d03"}},
        {0x21e80e03, {"Monk Rune of Superior Smiting Prayers", "0815b521e80e03"}},
        {0x21e80f03, {"Monk Rune of Superior Protection Prayers", "0815b521e80f03"}},
        {0x21e81003, {"Monk Rune of Superior Divine Favor", "0815b521e81003"}},
        {0x21e81102, {"Warrior Rune of Major Strength", "0815b621e81102"}},
        {0x21e81202, {"Warrior Rune of Major Axe Mastery", "0815b621e81202"}},
        {0x21e81302, {"Warrior Rune of Major Hammer Mastery", "0815b621e81302"}},
        {0x21e81402, {"Warrior Rune of Major Swordsmanship", "0815b621e81402"}},
        {0x21e81502, {"Warrior Rune of Major Tactics", "0815b621e81502"}},
        {0x27e902ea, {"Warrior Rune of Major Absorption", "0815b627e902ea"}},
        {0x21e81103, {"Warrior Rune of Superior Strength", "0815b721e81103"}},
        {0x21e81203, {"Warrior Rune of Superior Axe Mastery", "0815b721e81203"}},
        {0x21e81303, {"Warrior Rune of Superior Hammer Mastery", "0815b721e81303"}},
        {0x21e81403, {"Warrior Rune of Superior Swordsmanship", "0815b721e81403"}},
        {0x21e81503, {"Warrior Rune of Superior Tactics", "0815b721e81503"}},
        {0x27ea02ea, {"Warrior Rune of Superior Absorption", "0815b727ea02ea"}},
        {0x21e81602, {"Ranger Rune of Major Beast Mastery", "0815b821e81602"}},
        {0x21e81702, {"Ranger Rune of Major Expertise", "0815b821e81702"}},
        {0x21e81802, {"Ranger Rune of Major Wilderness Survival", "0815b821e81802"}},
        {0x21e81902, {"Ranger Rune of Major Marksmanship", "0815b821e81902"}},
        {0x21e81603, {"Ranger Rune of Superior Beast Mastery", "0815b921e81603"}},
        {0x21e81703, {"Ranger Rune of Superior Expertise", "0815b921e81703"}},
        {0x21e81803, {"Ranger Rune of Superior Wilderness Survival", "0815b921e81803"}},
        {0x21e81903, {"Ranger Rune of Superior Marksmanship", "0815b921e81903"}},
        {0x21e81d01, {"Assassin Rune of Minor Dagger Mastery", "0818b421e81d01"}},
        {0x21e81e01, {"Assassin Rune of Minor Deadly Arts", "0818b421e81e01"}},
        {0x21e81f01, {"Assassin Rune of Minor Shadow Arts", "0818b421e81f01"}},
        {0x21e82301, {"Assassin Rune of Minor Critical Strikes", "0818b421e82301"}},
        {0x21e81d02, {"Assassin Rune of Major Dagger Mastery", "0818b521e81d02"}},
        {0x21e81e02, {"Assassin Rune of Major Deadly Arts", "0818b521e81e02"}},
        {0x21e81f02, {"Assassin Rune of Major Shadow Arts", "0818b521e81f02"}},
        {0x21e82302, {"Assassin Rune of Major Critical Strikes", "0818b521e82302"}},
        {0x21e81d03, {"Assassin Rune of Superior Dagger Mastery", "0818b621e81d03"}},
        {0x21e81e03, {"Assassin Rune of Superior Deadly Arts", "0818b621e81e03"}},
        {0x21e81f03, {"Assassin Rune of Superior Shadow Arts", "0818b621e81f03"}},
        {0x21e82303, {"Assassin Rune of Superior Critical Strikes", "0818b621e82303"}},
        {0x21e82001, {"Ritualist Rune of Minor Communing", "0818b721e82001"}},
        {0x21e82101, {"Ritualist Rune of Minor Restoration Magic", "0818b721e82101"}},
        {0x21e82201, {"Ritualist Rune of Minor Channeling Magic", "0818b721e82201"}},
        {0x21e82401, {"Ritualist Rune of Minor Spawning Power", "0818b721e82401"}},
        {0x21e82002, {"Ritualist Rune of Major Communing", "0818b821e82002"}},
        {0x21e82102, {"Ritualist Rune of Major Restoration Magic", "0818b821e82102"}},
        {0x21e82202, {"Ritualist Rune of Major Channeling Magic", "0818b821e82202"}},
        {0x21e82402, {"Ritualist Rune of Major Spawning Power", "0818b821e82402"}},
        {0x21e82003, {"Ritualist Rune of Superior Communing", "0818b921e82003"}},
        {0x21e82103, {"Ritualist Rune of Superior Restoration Magic", "0818b921e82103"}},
        {0x21e82203, {"Ritualist Rune of Superior Channeling Magic", "0818b921e82203"}},
        {0x21e82403, {"Ritualist Rune of Superior Spawning Power", "0818b921e82403"}},
        {0x21e82901, {"Dervish Rune of Minor Scythe Mastery", "083cb921e82901"}},
        {0x21e82a01, {"Dervish Rune of Minor Wind Prayers", "083cb921e82a01"}},
        {0x21e82b01, {"Dervish Rune of Minor Earth Prayers", "083cb921e82b01"}},
        {0x21e82c01, {"Dervish Rune of Minor Mysticism", "083cb921e82c01"}},
        {0x21e82902, {"Dervish Rune of Major Scythe Mastery", "083cba21e82902"}},
        {0x21e82a02, {"Dervish Rune of Major Wind Prayers", "083cba21e82a02"}},
        {0x21e82b02, {"Dervish Rune of Major Earth Prayers", "083cba21e82b02"}},
        {0x21e82c02, {"Dervish Rune of Major Mysticism", "083cba21e82c02"}},
        {0x21e82903, {"Dervish Rune of Superior Scythe Mastery", "083cbb21e82903"}},
        {0x21e82a03, {"Dervish Rune of Superior Wind Prayers", "083cbb21e82a03"}},
        {0x21e82b03, {"Dervish Rune of Superior Earth Prayers", "083cbb21e82b03"}},
        {0x21e82c03, {"Dervish Rune of Superior Mysticism", "083cbb21e82c03"}},
        {0x21e82501, {"Paragon Rune of Minor Spear Mastery", "083cbc21e82501"}},
        {0x21e82601, {"Paragon Rune of Minor Command", "083cbc21e82601"}},
        {0x21e82701, {"Paragon Rune of Minor Motivation", "083cbc21e82701"}},
        {0x21e82801, {"Paragon Rune of Minor Leadership", "083cbc21e82801"}},
        {0x21e82502, {"Paragon Rune of Major Spear Mastery", "083cbd21e82502"}},
        {0x21e82602, {"Paragon Rune of Major Command", "083cbd21e82602"}},
        {0x21e82702, {"Paragon Rune of Major Motivation", "083cbd21e82702"}},
        {0x21e82802, {"Paragon Rune of Major Leadership", "083cbd21e82802"}},
        {0x21e82503, {"Paragon Rune of Superior Spear Mastery", "083cbe21e82503"}},
        {0x21e82603, {"Paragon Rune of Superior Command", "083cbe21e82603"}},
        {0x21e82703, {"Paragon Rune of Superior Motivation", "083cbe21e82703"}},
        {0x21e82803, {"Paragon Rune of Superior Leadership", "083cbe21e82803"}},
        {0xa53003bc, {"Vanguard's Insignia [Assassin]", "084ab4a53003bc"}},
        {0xa53003be, {"Infiltrator's Insignia [Assassin]", "084ab5a53003be"}},
        {0xa53003c0, {"Saboteur's Insignia [Assassin]", "084ab6a53003c0"}},
        {0xa53003c2, {"Nightstalker's Insignia [Assassin]", "084ab7a53003c2"}},
        {0xa53003c4, {"Artificer's Insignia [Mesmer]", "084ab8a53003c4"}},
        {0xa53003c6, {"Prodigy's Insignia [Mesmer]", "084ab9a53003c6"}},
        {0xa53003c8, {"Virtuoso's Insignia [Mesmer]", "084abaa53003c8"}},
        {0x253003ca, {"Radiant Insignia", "084abb253003ca"}},
        {0x253003cc, {"Survivor Insignia", "084abc253003cc"}},
        {0xa53003ce, {"Stalwart Insignia", "084abda53003ce"}},
        {0xa53003d0, {"Brawler's Insignia", "084abea53003d0"}},
        {0xa53003d2, {"Blessed Insignia", "084abfa53003d2"}},
        {0xa53003d4, {"Herald's Insignia", "084ac0a53003d4"}},
        {0xa53003d6, {"Sentry's Insignia", "084ac1a53003d6"}},
        {0x27e802a9, {"Bloodstained Insignia [Necromancer]", "084ac227e802a9"}},
        {0x253003d8, {"Tormentor's Insignia [Necromancer]", "084ac3253003d8"}},
        {0xa53003da, {"Undertaker's Insignia [Necromancer]", "084ac4a53003da"}},
        {0xa53003dc, {"Bonelace Insignia [Necromancer]", "084ac5a53003dc"}},
        {0xa53003de, {"Minion Master's Insignia [Necromancer]", "084ac6a53003de"}},
        {0xa53003e0, {"Blighter's Insignia [Necromancer]", "084ac7a53003e0"}},
        {0xa53003e2, {"Prismatic Insignia [Elementalist]", "084ac8a53003e2"}},
        {0xa53003e4, {"Hydromancer Insignia [Elementalist]", "084ac9a53003e4"}},
        {0xa53003e6, {"Geomancer Insignia [Elementalist]", "084acaa53003e6"}},
        {0xa53003e8, {"Pyromancer Insignia [Elementalist]", "084acba53003e8"}},
        {0xa53003ea, {"Aeromancer Insignia [Elementalist]", "084acca53003ea"}},
        {0xa53003ec, {"Wanderer's Insignia [Monk]", "084acda53003ec"}},
        {0xa53003ee, {"Disciple's Insignia [Monk]", "084acea53003ee"}},
        {0xa53003f0, {"Anchorite's Insignia [Monk]", "084acfa53003f0"}},
        {0xa53003f2, {"Knight's Insignia [Warrior]", "084ad0a53003f2"}},
        {0x27e802b6, {"Lieutenant's Insignia [Warrior]", "084ad127e802b6"}},
        {0x27e802b7, {"Stonefist Insignia [Warrior]", "084ad227e802b7"}},
        {0xa53003f4, {"Dreadnought Insignia [Warrior]", "084ad3a53003f4"}},
        {0xa53003f6, {"Sentinel's Insignia [Warrior]", "084ad4a53003f6"}},
        {0xa53003f8, {"Frostbound Insignia [Ranger]", "084ad5a53003f8"}},
        {0xa53003fa, {"Earthbound Insignia [Ranger]", "084ad6a53003fa"}},
        {0xa53003fc, {"Pyrebound Insignia [Ranger]", "084ad7a53003fc"}},
        {0xa53003fe, {"Stormbound Insignia [Ranger]", "084ad8a53003fe"}},
        {0xa5300400, {"Beastmaster's Insignia [Ranger]", "084ad9a5300400"}},
        {0xa5300402, {"Scout's Insignia [Ranger]", "084adaa5300402"}},
        {0xa5300404, {"Windwalker Insignia [Dervish]", "084adba5300404"}},
        {0xa5300406, {"Forsaken Insignia [Dervish]", "084adca5300406"}},
        {0xa5300408, {"Shaman's Insignia [Ritualist]", "084adda5300408"}},
        {0xa530040a, {"Ghost Forge Insignia [Ritualist]", "084adea530040a"}},
        {0xa530040c, {"Mystic's Insignia [Ritualist]", "084adfa530040c"}},
        {0xa530040e, {"Centurion's Insignia [Paragon]", "084ae0a530040e"}},
        {0x24d00201, {"Vial of Dye [Blue]", "0a009224d00201"}},
        {0x24d00301, {"Vial of Dye [Green]", "0a009224d00301"}},
        {0x24d00401, {"Vial of Dye [Purple]", "0a009224d00401"}},
        {0x24d00501, {"Vial of Dye [Red]", "0a009224d00501"}},
        {0x24d00601, {"Vial of Dye [Yellow]", "0a009224d00601"}},
        {0x24d00701, {"Vial of Dye [Brown]", "0a009224d00701"}},
        {0x24d00801, {"Vial of Dye [Orange]", "0a009224d00801"}},
        {0x24d00901, {"Vial of Dye [Silver]", "0a009224d00901"}},
        {0x24d00a01, {"Vial of Dye [Black]", "0a009224d00a01"}},
        {0x24d00c01, {"Vial of Dye [White]", "0a009224d00c01"}},
        {0x24d00d01, {"Vial of Dye [Pink]", "0a009224d00d01"}},
        {0x000b0399, {"Bone", "0b0399"}},
        {0x000b039a, {"Lump of Charcoal", "0b039a"}},
        {0x000b039b, {"Monstrous Claw", "0b039b"}},
        {0x000b039d, {"Bolt of Cloth", "0b039d"}},
        {0x000b039e, {"Bolt of Linen", "0b039e"}},
        {0x000b039f, {"Bolt of Damask", "0b039f"}},
        {0x000b03a0, {"Bolt of Silk", "0b03a0"}},
        {0x000b03a1, {"Pile of Glittering Dust", "0b03a1"}},
        {0x000b03a2, {"Glob of Ectoplasm", "0b03a2"}},
        {0x000b03a3, {"Monstrous Eye", "0b03a3"}},
        {0x000b03a4, {"Monstrous Fang", "0b03a4"}},
        {0x000b03a5, {"Feather", "0b03a5"}},
        {0x000b03a6, {"Plant Fiber", "0b03a6"}},
        {0x000b03a7, {"Diamond", "0b03a7"}},
        {0x000b03a8, {"Onyx Gemstone", "0b03a8"}},
        {0x000b03a9, {"Ruby", "0b03a9"}},
        {0x000b03aa, {"Sapphire", "0b03aa"}},
        {0x000b03ab, {"Tempered Glass Vial", "0b03ab"}},
        {0x000b03ac, {"Tanned Hide Square", "0b03ac"}},
        {0x000b03ad, {"Fur Square", "0b03ad"}},
        {0x000b03ae, {"Leather Square", "0b03ae"}},
        {0x000b03af, {"Elonian Leather Square", "0b03af"}},
        {0x000b03b0, {"Vial of Ink", "0b03b0"}},
        {0x000b03b1, {"Obsidian Shard", "0b03b1"}},
        {0x000b03b2, {"Wood Plank", "0b03b2"}},
        {0x000b03b4, {"Iron Ingot", "0b03b4"}},
        {0x000b03b5, {"Steel Ingot", "0b03b5"}},
        {0x000b03b6, {"Deldrimor Steel Ingot", "0b03b6"}},
        {0x000b03b7, {"Roll of Parchment", "0b03b7"}},
        {0x000b03b8, {"Roll of Vellum", "0b03b8"}},
        {0x000b03b9, {"Scale", "0b03b9"}},
        {0x000b03ba, {"Chitin Fragment", "0b03ba"}},
        {0x000b03bb, {"Granite Slab", "0b03bb"}},
        {0x000b03bc, {"Spiritwood Plank", "0b03bc"}},
        {0x000b1984, {"Amber Chunk", "0b1984"}},
        {0x000b1985, {"Jadeite Shard", "0b1985"}},
    };

    bool IsCommonMaterial(const GW::Item* item) {
        if (item && item->GetModifier(0x2508))
            return item->GetModifier(0x2508)->arg1() <= std::to_underlying(GW::Constants::MaterialSlot::Feather);
        return false;
    }
    float GetPriceById(const char* id)
    {
        const auto prices = PriceCheckerModule::FetchPrices();
        const auto found = prices.find(id);
        if (found != prices.end()) {
            return static_cast<float>(found->second);
        }
        return .0f;
    }
    float GetPriceByItem(const GW::Item* item, std::string* item_name_out = nullptr, unsigned int mod_start_index = 0) {
        if (item->type == GW::Constants::ItemType::Materials_Zcoins) {
            uint32_t mod = (std::to_underlying(item->type) << 16) | (item->model_id & 0xffff);
            const auto found = price_info_by_unique_mod_struct.find(mod);
            if (found == price_info_by_unique_mod_struct.end()) 
                return 0.f;
            *item_name_out = found->second.name;
            return GetPriceById(found->second.id);
        }

        size_t found_count = 0;
        for (size_t i = 0; i < item->mod_struct_size; i++) {
            const auto mod = item->mod_struct[i].mod;
            const auto found = price_info_by_unique_mod_struct.find(mod);
            if (found == price_info_by_unique_mod_struct.end()) 
                continue;
            if (found_count == mod_start_index) {
                *item_name_out = found->second.name;
                return GetPriceById(found->second.id);
            }
            found_count++;
        }
        return 0.f;
    }



    std::wstring PrintPrice(const uint32_t price, const char* name = nullptr) {
        auto color = GW::EncStrings::ItemCommon;
        if (price > high_price_threshold) {
            color = GW::EncStrings::ItemRare;
        }

        const uint32_t plat = price / 1000;
        const uint32_t gold = price % 1000;

        std::wstring currency_string;
        if (price == 0.f) {
            // 0 gold
            currency_string = L"\xAC2\x100";
        }
        else if (plat > 0 && gold > 0) {
            // N platinum, N gold
            currency_string = std::format(L"\xAC4\x101{}\x102{}", (wchar_t)(0x100 + plat), (wchar_t)(0x100 + gold));
        }
        else if(gold > 0) {
            // N gold
            currency_string = std::format(L"\xAC2\x101{}", (wchar_t)(0x100 + gold));
        }
        else {
            // N platinum
            currency_string = std::format(L"\xAC3\x101{}", (wchar_t)(0x100 + plat));
        }

        std::wstring subject;
        if (name && *name) {
            subject = std::format(L"\x108\x107{}\x1", TextUtils::StringToWString(name));
        }
        else {
            subject = L"\x108\x107Item price\x1";
        }

        return std::format(L"\x2\x102\x2{}\x10A\xA8A\x10A{}\x1\x10B{}\x1\x1", color, subject, currency_string);
    }
    std::wstring PrintPrice(float price, const char* name = nullptr) {
        if (price < .0f)
            price = .0f;
#pragma warning( push )
#pragma warning( disable : 4244)
        return PrintPrice(static_cast<uint32_t>(price), name);
#pragma warning( pop ) 
    }
    void UpdateDescription(const uint32_t item_id, std::wstring& description)
    {
        const auto item = GW::Items::GetItemById(item_id);
        if (!item)
            return;

        if (description.empty())
            description += L"\x101";

        std::string first_item_name;
        std::string second_item_name;
        const auto price_first = GetPriceByItem(item, &first_item_name, 0);
        const auto price_second = GetPriceByItem(item, &second_item_name, 1);
        if (price_first >= .1f) {
            description.append(PrintPrice(price_first, first_item_name.empty() ? nullptr : first_item_name.c_str()));
        }
        if (price_second >= .1f && first_item_name != second_item_name) {
            description.append(PrintPrice(price_second, second_item_name.empty() ? nullptr : second_item_name.c_str()));
        }
    }
    std::wstring tmp_item_description;
    void OnGetItemDescription(const uint32_t item_id, uint32_t, uint32_t, uint32_t, wchar_t**, wchar_t** out_desc)
    {
        if (!(out_desc && *out_desc)) return;
        if (*out_desc != tmp_item_description.data()) {
            tmp_item_description.assign(*out_desc);
        }
        UpdateDescription(item_id, tmp_item_description);
        *out_desc = tmp_item_description.data();
    }
}


void PriceCheckerModule::Initialize()
{
    ToolboxModule::Initialize();

    ItemDescriptionHandler::RegisterDescriptionCallback(OnGetItemDescription, 100);

    FetchPrices();
}

void PriceCheckerModule::Terminate()
{
    ToolboxModule::Terminate();

    ItemDescriptionHandler::UnregisterDescriptionCallback(OnGetItemDescription);
}


void PriceCheckerModule::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    SAVE_FLOAT(high_price_threshold);
}

void PriceCheckerModule::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    LOAD_FLOAT(high_price_threshold);
}

void PriceCheckerModule::DrawSettingsInternal()
{
    ImGui::SliderFloat("Price Checker high price threshold", &high_price_threshold, 100, 50000);
    ImGui::ShowHelp("Rune and mod prices are fetched from https://kamadan.gwtoolbox.com.\nWhen an item price is found to be above this price threshold, the mod price will be gold when an item is hovered.");
}

const std::unordered_map<std::string,uint32_t>& PriceCheckerModule::FetchPrices() {
    if (TIMER_DIFF(last_request_time) > request_interval) {
        last_request_time = TIMER_INIT();
        Resources::Download(trader_quotes_url, [](bool success, const std::string& response, void*) {
            if (success)
                ParsePriceJson(response);
            });
    }
    return prices_by_identifier;
}
