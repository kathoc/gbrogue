#!/usr/bin/env python3
"""
Generate the bilingual string tables + Misaki glyph bank.

- Messages/UI strings: (SID, en, ja) — ja is kana-centric, GB style.
  "%s" marks the single argument slot (encoded as byte 0x01).
- Name tables (monsters, item aliases/real names, weapons, armor, food)
  get JA counterparts with a fixed record stride for banked access.
- Every non-ASCII char used by JA text is harvested, given a code
  0x80+, and its 8x8 glyph is pulled from third_party/misaki (BDF).

Outputs:
  assets/lang_data.c   (#pragma bank 3: glyphs + string blobs + tables)
  src/lang_ids.h       (SID enum + table sizes)
  tests/lang_map.py    (code->char and SID->(en,ja) for the harness)
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BDF = ROOT / "third_party" / "misaki" / "misaki_gothic.bdf"

# ------------------------------------------------------- button icons
# The face/system buttons are drawn as inline 8x8 glyphs so operation
# hints and the how-to-play manual can show the actual button shape
# instead of the letters A/B/START/SELECT. They ride the same full-width
# glyph path as kana (a code >= 0x80 renders as one 8x8 tile at ink
# color), so a hint string just embeds these sentinel characters.
#
# Source art: samples/{A,B,START,SELECT}button.png. Each button body is
# opaque with the letter knocked out; here the body is the ink (bit set)
# and the knockout shows the label. START/SELECT are 16x8 = two tiles.
# These bitmaps are baked from those PNGs (alpha>=0x80 -> ink).
_A, _B = chr(0xE000), chr(0xE001)
_STL, _STR = chr(0xE002), chr(0xE003)
_SEL, _SER = chr(0xE004), chr(0xE005)
BTN_A  = _A
BTN_B  = _B
BTN_ST = _STL + _STR      # START: two 8x8 tiles
BTN_SE = _SEL + _SER      # SELECT: two 8x8 tiles
BUTTON_GLYPHS = {
    _A:   [0x7C, 0xE6, 0xDA, 0xC2, 0xDA, 0xDA, 0x7C, 0x00],  # A
    _B:   [0x7C, 0xC6, 0xDA, 0xC6, 0xDA, 0xC6, 0x7C, 0x00],  # B
    _STL: [0x7F, 0xE0, 0xDD, 0xC5, 0xF5, 0xCD, 0x7F, 0x00],  # START L
    _STR: [0xFC, 0x22, 0x56, 0x36, 0x56, 0x56, 0xFC, 0x00],  # START R
    _SEL: [0x7F, 0xE4, 0xDD, 0xC4, 0xF5, 0xCC, 0x7F, 0x00],  # SELECT L
    _SER: [0xFC, 0x5E, 0xDE, 0xDE, 0xDE, 0x46, 0xFC, 0x00],  # SELECT R
}
# Harness-side readable stand-ins for the decode map (tests/lang_map.py).
BUTTON_LABEL = {
    _A: "A", _B: "B",
    _STL: "[", _STR: "]",   # START tiles
    _SEL: "{", _SER: "}",   # SELECT tiles
}

# ------------------------------------------------------------- messages
S = [
    # core / title
    ("TITLE_SUB",      "a rogue clone",        "ろーぐくろーん"),
    ("TITLE_NEW",      "NEW GAME",             "はじめる"),
    ("TITLE_CONT",     "CONTINUE",             "つづきから"),
    ("MENU_RANK",      "RANKING",              "ランキング"),
    ("TITLE_SEED",     "SEED",                 "シード"),
    ("RANK_TITLE",     "- RANKING -",          "- ランキング -"),
    ("RANK_LEGEND",    "rank G=gold B=floor cause plays", "G:きんか B:ちか しいん プレイ"),
    ("RANK_EMPTY",     "no records yet",       "きろくなし"),
    ("RANK_ESCAPED",   "escaped!",             "だっしゅつ!"),
    ("RANK_PLAY",      "play",                 "かい"),
    ("WELCOME",        "Welcome to the tomb",  "はかばに ようこそ"),
    ("WELCOME_BACK",   "Welcome back",         "おかえりなさい"),
    ("YOU_WAIT",       "You wait",             "あしぶみした"),
    ("DESCEND",        "You descend...",       "かいだんを おりた"),
    ("CLIMB",          "You climb up...",      "かいだんを のぼった"),
    ("DIED",           "YOU  HAVE  DIED",      "あなたは しにました"),
    ("RIP",            "R.I.P. ...",            "ごめいふくを……"),
    ("ON_LEVEL",       "on level ",            "ちか "),
    ("WON",            "YOU  MADE  IT!",       "だっしゅつ せいこう!"),
    ("WON_SUB",        "The Amulet is yours",  "まよけは あなたのもの"),
    ("VICTORY",        "*** VICTORY ***",      "*** しょうり ***"),
    ("SAVED",          "Game saved.",          "セーブしました"),
    ("SAFE_OFF",       "Safe to power off.",   "でんげんを きってもOK"),
    ("PLUNGE1",        "You plunged through",  "おとしあなだ!"),
    ("PLUNGE2",        "a trap door!",         "したのかいへ らっか!"),
    ("LAND_HARD",      "You land hard",        "したたかに おちた"),
    ("TRAP_A",         f"{BTN_A}",             f"{BTN_A}"),
    ("AMULET_GET",     "The Amulet of Yendor!", "イェンダーのまよけだ!"),
    # combat
    ("YOU_HIT",        "You hit the %s",       "%sに いちげき!"),
    ("YOU_MISS",       "You miss the %s",      "%sに かわされた"),
    ("YOU_KILLED",     "You killed the %s",    "%sを たおした!"),
    ("MON_HITS",       "The %s hits you",      "%sの こうげき!"),
    ("MON_MISSES",     "The %s misses",        "%sは はずした"),
    ("WELCOME_LVL",    "Welcome to level ",    "レベルが あがった! Lv"),
    # items / pack
    ("GOT",            "Got %s",               "%sを ひろった"),
    ("PACK_FULL",      "Your pack is full",    "もちものが いっぱいだ"),
    ("FOUND_GOLD",     "You found %s gold",    "きんかを %sまい ひろった"),
    ("PACK_TITLE",     "PACK      gold ",      "もちもの   きんか "),
    ("PACK_HINT",      f"{BTN_A}:pick  {BTN_SE}/{BTN_B}:close",
                       f"{BTN_A}:えらぶ {BTN_SE}/{BTN_B}:とじる"),
    ("EMPTY_SLOT",     "Empty slot",           "なにもない"),
    ("DROPPED",        "Dropped",              "ゆかに おいた"),
    ("SOMETHING_HERE", "Something is here",    "ここには なにかある"),
    ("NO_ROOM",        "No room here",         "ここには おけない"),
    ("REMOVE_FIRST",   "Remove it first",      "そうびを はずしてから"),
    ("CURSED",         "It is cursed!",        "のろわれている!"),
    ("WIELD",          "You wield it",         "ぶきを かまえた"),
    ("UNWIELD",        "You unwield it",       "ぶきを おさめた"),
    ("WEAR",           "You put it on",        "よろいを きた"),
    ("TAKEOFF",        "You take it off",      "よろいを ぬいだ"),
    ("ALREADY_ARMOR",  "Already wearing armor", "もう よろいを きている"),
    ("RING_ON",        "You slip it on",       "ゆびわを はめた"),
    ("RING_OFF",       "You remove the ring",  "ゆびわを はずした"),
    ("HANDS_FULL",     "Both hands are full",  "りょうては ふさがっている"),
    ("YUM",            "Yum, that was good",   "おいしかった"),
    ("AMULET_GLOW",    "It glows softly",      "ほのかに ひかっている"),
    # potions
    ("P_CONF",         "Wait, what's going on?", "め が まわる.."),
    ("P_HALLU",        "Oh wow, everything...", "せかいが ゆがんで みえる"),
    ("P_SICK",         "You feel very sick",   "ぐあいが わるくなった"),
    ("P_SICK_M",       "You feel momentarily sick", "いっしゅん むかついた"),
    ("P_STR",          "You feel stronger!",   "ちからが わいてきた!"),
    ("P_SEEINV",       "Your eyes tingle",     "めが ぴりぴりする"),
    ("P_HEAL",         "You feel better",      "きずが いえた"),
    ("P_SEEMON",       "You sense company...", "けはいを かんじる.."),
    ("P_SEEMAGIC",     "You sense magic nearby", "まほうの けはいがする"),
    ("P_RAISE",        "You feel more skillful", "けいけんが ふえた"),
    ("P_HASTE",        "You feel yourself moving faster", "うごきが はやくなった"),
    ("P_FIXSTR",       "Your strength returns", "ちからが もどった"),
    ("P_BLIND",        "A cloak of darkness!", "めのまえが まっくらだ!"),
    ("P_LEVIT",        "You float in the air", "からだが うきあがった"),
    # scrolls
    ("S_CONFHIT",      "Your hands glow red",  "てが あかく ひかった"),
    ("S_MAP",          "The dungeon reveals!", "ちずが あたまに うかんだ!"),
    ("S_HOLD",         "They freeze!",         "てきが かなしばりに!"),
    ("S_FLEE",         "They flee!",           "てきが にげだした!"),
    ("S_NOTHING",      "Nothing happens",      "なにも おこらない"),
    ("S_SLEEP",        "You fall asleep!",     "ねむってしまった!"),
    ("S_ARMOR",        "Your armor glows",     "よろいが ひかった"),
    ("S_SKIN",         "Your skin tingles",    "はだが ぴりぴりした"),
    ("S_ID",           "It is %s",             "それは %s だった"),
    ("S_SMART",        "You feel smart",       "かしこくなった きがする"),
    ("S_FOOD",         "You smell food...",    "たべものの においがする"),
    ("S_TELE",         "You are elsewhere!",   "べつのばしょに とんだ!"),
    ("S_WEAPON",       "Your weapon glows blue", "ぶきが あおく ひかった"),
    ("S_MAKEMON",      "A monster appears!",   "モンスターが あらわれた!"),
    ("S_GROWL",        "You hear a faint growl", "うなりごえが きこえる"),
    ("S_PROTECT",      "You feel protected",   "まもられている きがする"),
    ("S_ROAR",         "You hear a loud roar", "おおきな ほうこうが した"),
    # wands
    ("W_WHICHWAY",     "Which way?",           "どのほうこう?"),
    ("W_GLOW",         "The corridor glows",   "つうろが ひかった"),
    ("W_VANISH",       "It vanishes!",         "てきが きえた!"),
    ("W_BOLT",         "A bolt of lightning!", "いなずまが はしる!"),
    ("W_FLAME",        "A burst of flame!",    "ほのおが ふきだす!"),
    ("W_ICE",          "An icy blast!",        "れいきが ふきつける!"),
    ("W_FIZZLE",       "The bolt fizzles",     "ふはつに おわった"),
    ("W_POLY",         "It changes shape!",    "てきが へんしんした!"),
    ("W_MISSILE",      "A magic missile!",     "まほうだん めいちゅう!"),
    ("W_HASTE",        "It speeds up!",        "てきが はやくなった!"),
    ("W_SLOW",         "It slows down",        "てきが おそくなった"),
    ("W_DRAIN",        "You feel life drain out", "せいきを すいとられた"),
    ("W_WEAK",         "You are too weak",     "よわりすぎている"),
    ("W_AWAY",         "It disappears!",       "てきが とばされた!"),
    ("W_TO",           "It appears beside you!", "てきが よびよせられた!"),
    ("W_DULL",         "It looks duller",      "てきの まりょくが きえた"),
    # monsters' abilities
    ("A_RUST",         "Your armor weakens!",  "よろいが さびた!"),
    ("A_FROZEN",       "You are frozen!",      "こおりついた!"),
    ("A_PURSE",        "Your purse feels light!", "さいふが かるくなった!"),
    ("A_STOLE",        "She stole something!", "なにかを ぬすまれた!"),
    ("A_WEAKER",       "You feel weaker",      "ちからが ぬけていく"),
    ("A_LIFE",         "Life drains away...",  "せいめいが すわれる.."),
    ("A_DRAINED",      "You feel drained...",  "けいけんが うばわれた.."),
    ("A_HELD",         "You are held fast!",   "つかまって うごけない!"),
    # traps / hunger
    ("T_BEAR",         "A bear trap grabs you", "トラばさみに かかった!"),
    ("T_GAS",          "Strange gas! You sleep", "ガスだ! ねむってしまう"),
    ("T_ARROW",        "An arrow hits you",    "やが ささった!"),
    ("T_ARROW_MISS",   "An arrow whizzes by",  "やが かすめた"),
    ("T_DART",         "A poison dart hits",   "どくばりが ささった!"),
    ("T_DART_MISS",    "A dart whizzes by",    "どくばりが かすめた"),
    ("T_FLOAT",        "You float over a trap", "わなのうえを とびこえた"),
    ("T_FOUND",        "You found a trap",     "わなを みつけた"),
    ("H_HUNGRY",       "You are hungry",       "おなかが すいてきた"),
    ("H_WEAK",         "You feel weak",        "くうふくで ふらふらだ"),
    ("H_FAINT",        "You are fainting",     "いしきが とおのく.."),
    ("H_STARVE",       "You starve...",        "うえじにした.."),
    ("H_FAINTED",      "You faint!",           "きぜつした!"),
    # throwing
    ("TH_NOTHING",     "Nothing to throw",     "なげるものがない"),
    ("TH_HIT",         "You hit it!",          "めいちゅう!"),
    ("TH_MISS",        "You miss it",          "はずれた"),
    ("TH_CLATTER",     "It clatters away",     "カラカラところがった"),
    # menu / ui
    ("MENU_TITLE",     "MENU",                 "メニュー"),
    ("MENU_REST",      "Rest a turn",          "やすむ"),
    ("MENU_SEARCH",    "Search",               "さがす"),
    ("MENU_THROW",     "Throw / fire",         "なげる"),
    ("MENU_DISPLAY",   "Display mode",         "がめんきりかえ"),
    ("MENU_LOG",       "Message log",          "ログ"),
    ("AIM_THROW",      "throw",                "なげる"),
    ("AIM_FIRE",       "fire",                 "はなつ"),
    ("AIM_ZAP",        "zap",                  "ふる"),
    ("AIM_DIR",        "which way?",           "ほうこうを せんたく"),
    ("MENU_SPEED",     "Repeat speed",         "リピートそくど"),
    ("MENU_LANG",      "Language: EN",         "げんご: にほんご"),
    ("MENU_QUIT",      "Save & quit",          "ちゅうだんする"),
    ("MENU_HINT",      f"{BTN_A}:ok  {BTN_B}:back", f"{BTN_A}:けってい {BTN_B}:もどる"),
    ("LOG_TITLE",      "LOG  (newest first)",  "きろく (あたらしいじゅん)"),
    ("LOG_HINT",       "B: back",              "B:もどる"),
    ("MAP_TITLE",      "MAP",                  "ちず"),
    ("POPUP_OK",       "        -A-",          "        -A-"),
    # --- appended (SIDs are positional: never reorder above) ---
    ("SPEED_SLOW",     "slow",                 "おそい"),
    ("SPEED_NORMAL",   "normal",               "ふつう"),
    ("SPEED_FAST",     "fast",                 "はやい"),
    # pack-screen descriptions: unknown templates + a learn hint
    ("D_UNK_PTN",      "a potion of unknown effect",  "こうかの わからない くすり"),
    ("D_UNK_SCR",      "a scroll of unknown effect",  "こうかの わからない まきもの"),
    ("D_UNK_WND",      "a wand of unknown effect",    "こうかの わからない つえ"),
    ("D_UNK_RNG",      "a ring of unknown effect",    "こうかの わからない ゆびわ"),
    ("D_TRYIT",        "(use it to find out)",        "(つかうと わかる)"),
    ("D_WEAPON",       "a weapon: wield it to fight", "ぶき: そうびして たたかう"),
    ("D_ARMOR",        "armor: wear it for defense",  "よろい: そうびして みをまもる"),
    ("D_FOOD",         "food: eases your hunger",     "たべると おなかが ふくれる"),
    ("D_AMULET",       "bring it back to the surface","ちじょうへ もちかえるべき ひほう"),
    # potions 0..13 (order = POTION_NAME)
    ("D_PTN0",  "confuses you for a while",     "のむと しばらく こんらんする"),
    ("D_PTN1",  "you see things...",            "げんかくが みえる"),
    ("D_PTN2",  "poison saps your strength",    "どく。ちからが さがる"),
    ("D_PTN3",  "raises your strength",         "ちからが あがる"),
    ("D_PTN4",  "reveals invisible things",     "みえないものが みえるようになる"),
    ("D_PTN5",  "heals your wounds",            "HPを かいふくする"),
    ("D_PTN6",  "senses monsters nearby",       "てきの いばしょが わかる"),
    ("D_PTN7",  "senses magic items",           "まほうの アイテムが わかる"),
    ("D_PTN8",  "raises your level",            "レベルが あがる"),
    ("D_PTN9",  "greatly heals your wounds",    "HPを おおきく かいふくする"),
    ("D_PTN10", "speeds you up",                "うごきが はやくなる"),
    ("D_PTN11", "restores lost strength",       "さがった ちからを もどす"),
    ("D_PTN12", "blinds you for a while",       "しばらく めが みえなくなる"),
    ("D_PTN13", "you float over the floor",     "からだが うきあがる"),
    # scrolls 0..12 (order = SCROLL_NAME)
    ("D_SCR0",  "your next hit confuses",       "つぎの いちげきで てきを こんらん"),
    ("D_SCR1",  "maps the whole level",         "フロアぜんたいの ちずが わかる"),
    ("D_SCR2",  "holds monsters in place",      "てきを そのばに かなしばり"),
    ("D_SCR3",  "reading it puts you to sleep", "よむと ねむってしまう"),
    ("D_SCR4",  "enchants your armor",          "よろいを きょうかする"),
    ("D_SCR5",  "identifies an item",           "アイテムを かんていする"),
    ("D_SCR6",  "scares monsters away",         "てきが おびえて にげだす"),
    ("D_SCR7",  "senses food on the level",     "しょくりょうの ばしょが わかる"),
    ("D_SCR8",  "teleports you away",           "どこかへ テレポートする"),
    ("D_SCR9",  "enchants your weapon",         "ぶきを きょうかする"),
    ("D_SCR10", "summons a monster",            "てきを よびよせて しまう"),
    ("D_SCR11", "removes curses",               "そうびの のろいを とく"),
    ("D_SCR12", "a loud noise wakes the level", "おおおとで てきが めをさます"),
    # wands 0..13 (order = WAND_NAME)
    ("D_WND0",  "lights up the room",           "へやを あかるくする"),
    ("D_WND1",  "turns a monster invisible",    "てきを とうめいに してしまう"),
    ("D_WND2",  "fires a lightning bolt",       "いなずまを はなつ"),
    ("D_WND3",  "fires a bolt of flame",        "ほのおを はなつ"),
    ("D_WND4",  "fires a bolt of frost",        "れいきを はなつ"),
    ("D_WND5",  "polymorphs a monster",         "てきを べつの すがたに かえる"),
    ("D_WND6",  "shoots a magic missile",       "まほうの たまを うつ"),
    ("D_WND7",  "hastes a monster (bad!)",      "てきが はやくなって しまう"),
    ("D_WND8",  "slows a monster",              "てきの うごきを おそくする"),
    ("D_WND9",  "trades your HP for damage",    "じぶんのHPで てきを うつ"),
    ("D_WND10", "nothing happens",              "なにも おこらない"),
    ("D_WND11", "knocks a monster away",        "てきを ふきとばす"),
    ("D_WND12", "pulls a monster to you",       "てきを ひきよせる"),
    ("D_WND13", "cancels a monster's powers",   "てきの とくしゅのうりょくを けす"),
    # rings 0..13 (order = RING_NAME)
    ("D_RNG0",  "protects you (better AC)",     "まもりが かたくなる"),
    ("D_RNG1",  "adds strength while worn",     "ちからが あがる"),
    ("D_RNG2",  "your strength cannot drop",    "ちからが さがらなくなる"),
    ("D_RNG3",  "helps you find traps",         "わなを みつけやすくなる"),
    ("D_RNG4",  "reveals invisible things",     "みえないものが みえるようになる"),
    ("D_RNG5",  "just pretty jewelry",          "ただの きれいな かざり"),
    ("D_RNG6",  "monsters come after you",      "てきが よってきて しまう"),
    ("D_RNG7",  "your blows land more often",   "こうげきが あたりやすくなる"),
    ("D_RNG8",  "your blows hit harder",        "こうげきが つよくなる"),
    ("D_RNG9",  "you heal faster",              "HPの かいふくが はやくなる"),
    ("D_RNG10", "you get hungry slower",        "おなかが へりにくくなる"),
    ("D_RNG11", "teleports you at random",      "かってに テレポートしてしまう"),
    ("D_RNG12", "sleeping monsters stay asleep","ねている てきに きづかれにくい"),
    ("D_RNG13", "your armor cannot rust",       "よろいが さびなくなる"),
    # status-row effect tags (order mirrors status.c EFF_TIMERS)
    ("ST_HASTE",  "hast", "速"),
    ("ST_CONF",   "conf", "混"),
    ("ST_BLIND",  "blnd", "盲"),
    ("ST_HALLUC", "hall", "幻"),
    ("ST_LEVIT",  "levt", "浮"),
    ("ST_SLEEP",  "slep", "眠"),
    ("ST_HELD",   "held", "縛"),
    ("ST_SEEINV", "sinv", "視"),
    ("ST_MONDET", "mdet", "感"),
    # bottom-right stairs hint
    # world status row is tight — no colon after the icon (saves a cell)
    ("HINT_DESCEND", f"{BTN_A}down", f"{BTN_A}おりる"),
    ("HINT_CLIMB",   f"{BTN_A}up",   f"{BTN_A}のぼる"),
    ("HINT_CANCEL",  f"{BTN_B}cancel", f"{BTN_B}やめる"),
    # after the Amulet: descend (A) or climb toward the surface (B)
    ("HINT_BOTH",    f"{BTN_A}down {BTN_B}up", f"{BTN_A}おりる {BTN_B}のぼる"),
    # post-Amulet the stairs hint alternates HINT_ASCEND (B) and
    # HINT_DESCEND (A) every 60 frames; ascend is climb-toward-surface
    ("HINT_ASCEND",  f"{BTN_B}up",   f"{BTN_B}のぼる"),
    # game-over causes (natural sentences, not raw log lines)
    ("DEATH_MON",    "Slain by the %s",       "%sに たおされた"),
    ("DEATH_ARROW",  "Killed by an arrow trap", "やのワナで しんだ"),
    ("DEATH_DART",   "Killed by a poison dart", "どくばりのワナで しんだ"),
    ("DEATH_STARVE", "Starved to death",      "うえて しんだ"),
    # pack action submenu: a 3-option vertical menu (primary verb / drop
    # / cancel) drawn where the item was described. Up/Down move, A picks,
    # B backs to the list, SELECT closes the pack. Verbs are bare words;
    # the menu code adds the "> " cursor and indents the block two columns
    # past the item list so it reads as a submenu. ACT_HINT is the row-17
    # button hint.
    ("ACT_WEAPON", "wield",  "そうび"),
    ("ACT_ARMOR",  "wear",   "そうび"),
    ("ACT_RING",   "put on", "はめる"),
    ("ACT_REMOVE", "remove", "はずす"),
    ("ACT_POTION", "drink",  "のむ"),
    ("ACT_SCROLL", "read",   "よむ"),
    ("ACT_WAND",   "aim",    "ふる"),
    ("ACT_FIRE",   "fire",   "はなつ"),
    ("ACT_THROW",  "throw",  "なげる"),
    ("ACT_FOOD",   "eat",    "たべる"),
    ("ACT_AMULET", "look",   "しらべる"),
    ("ACT_DROP",   "drop",   "おく"),
    ("ACT_CANCEL", "cancel", "やめる"),
    ("ACT_HINT",   f"{BTN_A}:ok {BTN_B}:back {BTN_SE}:close",
                   f"{BTN_A}:けってい {BTN_B}:もどる {BTN_SE}:とじる"),
    # full-map overview: reminder that B closes it (SELECT release does not)
    ("MAP_CLOSE",  f"{BTN_B}:close", f"{BTN_B}:とじる"),
    # START menu entry: opens the full-map overview (same as SELECT-hold)
    ("MENU_MAP",   "Map",  "マップ"),
    # how-to-play manual: menu label, screen title/hint, then HELP0..N
    # (a POSITIONAL block — ui_help.c loops SID_HELP0 .. SID_HELP0+N-1)
]

SIDS = [sid for sid, _, _ in S]

# ------------------------------------------------------- name tables (JA)
# Katakana names are chosen so each name's leading kana echoes the map
# glyph (always the English initial A..Z), e.g. K=ケストレル, R=ラトル
# スネーク. P/ファントム (Ph=F) and W/レイス have no kana that matches the
# letter's sound, so recognizability wins there.
MNAME_JA = [
    "アクアター", "バット", "ケンタウロス", "ドラゴン", "エミュー",
    "フライトラップ", "グリフォン", "ホブゴブリン", "アイスモンスター",
    "ジャバウォック", "ケストレル", "レプラコーン", "メドゥーサ",
    "ニンフ", "オーク", "ファントム", "クアッガ", "ラトルスネーク",
    "スネーク", "トロル", "ユニコーン", "ヴァンパイア", "レイス",
    "ゼロック", "イエティ", "ゾンビ",
]
POTION_ALIAS_JA = ["あか", "あお", "みどり", "とうめい", "こはく", "ちゃいろ",
                   "ピンク", "みずいろ", "きんいろ", "けむり", "にゅうはく",
                   "くろ", "すみれ", "うすちゃ"]
POTION_NAME_JA = ["こんらん", "まぼろし", "どく", "ちから", "とうし",
                  "かいふく", "てきかんち", "まほうかんち", "レベルアップ",
                  "だいかいふく", "しんそく", "ちからもどし", "もうもく", "ふゆう"]
SCROLL_ALIAS_JA = ["ゼルゴメル", "ジュイドオエ", "ナンバーナイン", "ジザクサ",
                   "プラチャバヤ", "ダイエンフォ", "レプゲクス", "プリルタ",
                   "エルビブヨロ", "ベルイエド", "ベンザール", "サール",
                   "ヤムヤム"]
SCROLL_NAME_JA = ["てきこんらん", "まほうのちず", "かなしばり", "すいみん",
                  "よろいきょうか", "かんてい", "おどかし", "しょくかんち",
                  "テレポート", "ぶききょうか", "しょうかん", "のろいけし",
                  "そうおん"]
WAND_ALIAS_JA = ["かし", "まつ", "てつ", "あえん", "しんちゅう", "ほね",
                 "ぞうげ", "チーク", "こくたん", "ルビー", "ひすい",
                 "めのう", "オパール", "はがね"]
WAND_NAME_JA = ["ひかり", "とうめい", "いなずま", "ほのお", "れいき",
                "へんしん", "まほうだん", "てきかそく", "てきげんそく",
                "すいとり", "はずれ", "てきとばし", "よびよせ", "むりょくか"]
RING_ALIAS_JA = ["ルビー", "トパーズ", "オニキス", "ひすい", "オパール",
                 "しんじゅ", "めのう", "さんご", "こはく", "りょくちゅう",
                 "とらめいし", "きせい", "みかげ", "すいしょう"]
RING_NAME_JA = ["まもり", "ちから", "ちからいじ", "たんさく", "とうし",
                "かざり", "そうおん", "めいちゅう", "いりょく", "さいせい",
                "はらもち", "テレポート", "しのびあし", "よろいいじ"]
WEAPON_NAME_JA = ["メイス", "ロングソード", "ゆみ", "や", "たんけん",
                  "だいけん", "ダーツ", "しゅりけん", "やり"]
ARMOR_NAME_JA = ["かわよろい", "リングメイル", "びょううち", "うろこ",
                 "くさりかたびら", "スプリント", "おびがね", "プレート"]
FOOD_NAME_JA = ["しょくりょう", "スライムモルド"]
# item kind suffixes: EN uses " ptn" etc; JA appends after the alias
KIND_SUFFIX_JA = {"PTN": "のくすり", "SCR": "のまきもの",
                  "WND": "のつえ", "RNG": "のゆびわ"}
EXTRA_JA = ["きんか", "まよけ", "???"]   # gold label, amulet, unknown

NAME_STRIDE = 16          # bytes per record incl. NUL

# ------------------------------------------------------- name tables (EN)
# English names also live in bank 3 (they used to be HOME const data;
# the flat-HOME 64KB layout leaves no room for code AND string tables).
MNAME_EN = ["aquator", "bat", "centaur", "dragon", "emu", "flytrap",
            "griffin", "hobgoblin", "ice monster", "jabberwock", "kestrel",
            "leprechaun", "medusa", "nymph", "orc", "phantom", "quagga",
            "rattlesnake", "snake", "troll", "unicorn", "vampire", "wraith",
            "xeroc", "yeti", "zombie"]
POTION_ALIAS_EN = ["red", "blue", "green", "clear", "amber", "brown",
                   "pink", "cyan", "gold", "smoky", "milky", "black",
                   "violet", "tan"]
POTION_NAME_EN = ["confusion", "hallucin.", "poison", "gain str",
                  "see invis", "healing", "see mons", "see magic",
                  "raise lvl", "ex-heal", "haste", "fix str",
                  "blindness", "levitate"]
SCROLL_ALIAS_EN = ["zelgo mer", "juyed awe", "nr 9", "xixaxa",
                   "pratyavaya", "daiyen fo", "lep gex", "priruta",
                   "elbib yloh", "verr yed", "venzar", "tharr", "yum yum"]
SCROLL_NAME_EN = ["conf mons", "magic map", "hold mons", "sleep",
                  "ench armor", "identify", "scare mons", "find food",
                  "teleport", "ench weapon", "make mons", "rm curse",
                  "aggravate"]
WAND_ALIAS_EN = ["oak", "pine", "iron", "zinc", "brass", "bone", "ivory",
                 "teak", "ebony", "ruby", "jade", "agate", "opal", "steel"]
WAND_NAME_EN = ["light", "invis", "lightning", "fire", "cold",
                "polymorph", "missile", "haste mon", "slow mon", "drain",
                "nothing", "tele away", "tele to", "cancel"]
RING_ALIAS_EN = ["ruby", "topaz", "onyx", "jade", "opal", "pearl",
                 "agate", "coral", "amber", "beryl", "tiger eye",
                 "wooden", "granite", "quartz"]
RING_NAME_EN = ["protect", "add str", "sust str", "search", "see invis",
                "adorn", "aggravate", "dexterity", "damage", "regen",
                "slow dig", "teleport", "stealth", "sust armor"]
WEAPON_NAME_EN = ["mace", "long sword", "short bow", "arrow", "dagger",
                  "2h sword", "dart", "shuriken", "spear"]
ARMOR_NAME_EN = ["leather", "ring mail", "studded", "scale", "chain",
                 "splint", "banded", "plate"]
FOOD_NAME_EN = ["ration", "slime mold"]
KIND_SUFFIX_EN = [" ptn", " scr", " wnd", " rng"]
EXTRA_EN = ["gold", "the Amulet", "???"]

# --------------------------------------------------------------- charset


def all_ja_text():
    for _, _, ja in S:
        yield ja
    for tbl in (MNAME_JA, POTION_ALIAS_JA, POTION_NAME_JA, SCROLL_ALIAS_JA,
                SCROLL_NAME_JA, WAND_ALIAS_JA, WAND_NAME_JA, RING_ALIAS_JA,
                RING_NAME_JA, WEAPON_NAME_JA, ARMOR_NAME_JA, FOOD_NAME_JA,
                EXTRA_JA):
        yield from tbl
    yield from KIND_SUFFIX_JA.values()


def harvest_charset():
    chars = set()
    for text in all_ja_text():
        for ch in text:
            if ord(ch) > 0x7E:
                chars.add(ch)
    chars.update(BUTTON_GLYPHS)   # icons always get a code + baked glyph
    return sorted(chars)


def parse_bdf(path):
    glyphs = {}
    cur = None
    rows = []
    in_bitmap = False
    for line in path.read_text(encoding="ascii", errors="ignore").splitlines():
        if line.startswith("ENCODING"):
            cur = int(line.split()[1])
        elif line.startswith("BITMAP"):
            in_bitmap = True
            rows = []
        elif line.startswith("ENDCHAR"):
            if cur is not None:
                rows = (rows + [0] * 8)[:8]
                glyphs[cur] = rows
            cur = None
            in_bitmap = False
        elif in_bitmap:
            rows.append(int(line.strip()[:2].ljust(2, "0"), 16))
    return glyphs


def glyph_code(ch):
    """misaki_gothic.bdf is ISO10646-encoded: Unicode codepoints."""
    return ord(ch)


def encode(text, cmap):
    out = bytearray()
    i = 0
    while i < len(text):
        ch = text[i]
        if text[i:i + 2] == "%s":
            out.append(0x01)
            i += 2
            continue
        if ord(ch) <= 0x7E:
            out.append(ord(ch))
        else:
            out.append(cmap[ch])
        i += 1
    return bytes(out)


def c_bytes(bs):
    return ", ".join(f"0x{b:02X}" for b in bs)


def code_for(i):
    """Glyph index -> string byte. 0x01 is the %s placeholder, so kana
    use 0x02..0x1F first, then 0x80..0xFF (158 slots total)."""
    return 0x02 + i if i < 30 else 0x80 + (i - 30)


def main():
    chars = harvest_charset()
    if len(chars) > 158:
        raise SystemExit(f"too many kana glyphs: {len(chars)}")
    cmap = {ch: code_for(i) for i, ch in enumerate(chars)}
    glyphs = parse_bdf(BDF)

    # ---- string blobs (EN then JA), offset tables
    def build_blob(idx):
        blob = bytearray()
        offs = []
        for row in S:
            offs.append(len(blob))
            blob += encode(row[idx], cmap) + b"\x00"
        blob += b"\x00" * 44          # far_copy overrun pad
        return blob, offs

    blob_en, offs_en = build_blob(1)
    blob_ja, offs_ja = build_blob(2)

    lines = [
        "/*",
        " * GENERATED FILE — see scripts/gen_lang.py",
        " *",
        " * Bilingual string tables + Misaki 8x8 kana glyphs (1bpp).",
        " * Misaki font (c) Num Kadoma — free license, see",
        " * third_party/misaki/misaki.txt.",
        " */",
        "#include <gb/gb.h>",
        "#include <stdint.h>",
        "",
        "#pragma bank 3",
        "",
        "BANKREF(lang_bank)",
        "const uint8_t lang_bank_anchor = 1;",
        "",
        f"const uint8_t MISAKI[{len(chars)}][8] = {{",
    ]
    SMALL_KANA = set("ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮ")
    for ch in chars:
        if ch in BUTTON_GLYPHS:      # button icon: baked 8x8, no centering
            lines.append(f"    {{ {c_bytes(BUTTON_GLYPHS[ch])} }},  /* btn */")
            continue
        rows = glyphs.get(glyph_code(ch))
        if rows is None:
            raise SystemExit(f"glyph missing in BDF: {ch!r}")
        if any(rows):
            if ch in SMALL_KANA:
                # small kana sit on the baseline (bottom-aligned)
                while rows[-1] == 0:
                    rows = [0] + rows[:-1]
            else:
                # everything else is vertically centered so a line of
                # mixed glyphs shares one visual midline
                top = next(i for i, r in enumerate(rows) if r)
                bot = 7 - max(i for i, r in enumerate(rows) if r)
                shift = (bot - top) // 2
                if shift > 0:      # push down
                    rows = [0] * shift + rows[:8 - shift]
                elif shift < 0:    # pull up
                    rows = rows[-shift:] + [0] * (-shift)
        lines.append(f"    {{ {c_bytes(rows)} }},  /* {ch} */")
    lines.append("};")
    lines.append("")
    lines.append(f"const uint8_t STR_BLOB_EN[{len(blob_en)}] = {{")
    for i in range(0, len(blob_en), 16):
        lines.append("    " + c_bytes(blob_en[i:i+16]) + ",")
    lines.append("};")
    lines.append(f"const uint8_t STR_BLOB_JA[{len(blob_ja)}] = {{")
    for i in range(0, len(blob_ja), 16):
        lines.append("    " + c_bytes(blob_ja[i:i+16]) + ",")
    lines.append("};")
    lines.append(f"const uint16_t STR_OFS_EN[{len(S)}] = {{")
    lines.append("    " + ", ".join(str(o) for o in offs_en) + ",")
    lines.append("};")
    lines.append(f"const uint16_t STR_OFS_JA[{len(S)}] = {{")
    lines.append("    " + ", ".join(str(o) for o in offs_ja) + ",")
    lines.append("};")

    def table(name, rows):
        lines.append(f"const uint8_t {name}[{len(rows)}][{NAME_STRIDE}] = {{")
        for text in rows:
            enc = encode(text, cmap)[:NAME_STRIDE - 1]
            enc = enc + b"\x00" * (NAME_STRIDE - len(enc))
            lines.append("    { " + c_bytes(enc) + " },")
        lines.append("};")

    table("MNAME_JA", MNAME_JA)
    table("POTION_ALIAS_JA", POTION_ALIAS_JA)
    table("POTION_NAME_JA", POTION_NAME_JA)
    table("SCROLL_ALIAS_JA", SCROLL_ALIAS_JA)
    table("SCROLL_NAME_JA", SCROLL_NAME_JA)
    table("WAND_ALIAS_JA", WAND_ALIAS_JA)
    table("WAND_NAME_JA", WAND_NAME_JA)
    table("RING_ALIAS_JA", RING_ALIAS_JA)
    table("RING_NAME_JA", RING_NAME_JA)
    table("WEAPON_NAME_JA", WEAPON_NAME_JA)
    table("ARMOR_NAME_JA", ARMOR_NAME_JA)
    table("FOOD_NAME_JA", FOOD_NAME_JA)
    table("KIND_SUFFIX_JA", [KIND_SUFFIX_JA[k] for k in ("PTN", "SCR", "WND", "RNG")])
    table("EXTRA_JA", EXTRA_JA)
    table("MNAME_EN", MNAME_EN)
    table("POTION_ALIAS_EN", POTION_ALIAS_EN)
    table("POTION_NAME_EN", POTION_NAME_EN)
    table("SCROLL_ALIAS_EN", SCROLL_ALIAS_EN)
    table("SCROLL_NAME_EN", SCROLL_NAME_EN)
    table("WAND_ALIAS_EN", WAND_ALIAS_EN)
    table("WAND_NAME_EN", WAND_NAME_EN)
    table("RING_ALIAS_EN", RING_ALIAS_EN)
    table("RING_NAME_EN", RING_NAME_EN)
    table("WEAPON_NAME_EN", WEAPON_NAME_EN)
    table("ARMOR_NAME_EN", ARMOR_NAME_EN)
    table("FOOD_NAME_EN", FOOD_NAME_EN)
    table("KIND_SUFFIX_EN", KIND_SUFFIX_EN)
    table("EXTRA_EN", EXTRA_EN)
    lines.append("")
    (ROOT / "assets" / "lang_data.c").write_text("\n".join(lines))

    # ---- SID header
    hdr = [
        "/* GENERATED FILE — see scripts/gen_lang.py */",
        "#ifndef LANG_IDS_H",
        "#define LANG_IDS_H",
        "",
    ]
    for i, sid in enumerate(SIDS):
        hdr.append(f"#define SID_{sid} {i}u")
    hdr += [
        "",
        f"#define SID_COUNT {len(SIDS)}u",
        f"#define LANG_KANA_COUNT {len(chars)}u",
        f"#define LANG_NAME_STRIDE {NAME_STRIDE}u",
        "",
        "#endif",
        "",
    ]
    (ROOT / "src" / "lang_ids.h").write_text("\n".join(hdr))

    # ---- harness map
    tm = [
        "# GENERATED FILE - see scripts/gen_lang.py",
        "KANA = {",
    ]
    def harness_text(s):
        for ch, lbl in BUTTON_LABEL.items():
            s = s.replace(ch, lbl)
        return s
    for ch, code in cmap.items():
        tm.append(f"    0x{code:02X}: {harness_text(ch)!r},")
    tm.append("}")
    tm.append("STRINGS = {")
    for sid, en, ja in S:
        tm.append(f"    {sid!r}: ({harness_text(en)!r}, {harness_text(ja)!r}),")
    tm.append("}")
    tm.append("")
    (ROOT / "tests" / "lang_map.py").write_text("\n".join(tm))

    print(f"lang: {len(S)} strings, {len(chars)} kana glyphs, "
          f"blob EN {len(blob_en)}B JA {len(blob_ja)}B")


if __name__ == "__main__":
    main()
