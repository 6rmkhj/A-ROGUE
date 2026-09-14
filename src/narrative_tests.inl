// Narrative integration tests run through public actions, not renderer timing.
// Included after smoke.cpp's shared fixtures to keep a single test executable.

static NarrativeProgress NarrativeReady() {
    NarrativeProgress progress = {};
    lstrcpyW(progress.playerName, L"서윤");
    progress.introSeen = 1;
    progress.tutorialSeen = 1;
    return progress;
}

static int TestNarrativeTutorial() {
    const uint32_t seed = 0xA703001u;
    GameState game, fresh;
    NewRun(&game, seed, 0); fresh = game;
    game.enemyScanned[0] = 1;
    NarrativeProgress progress = {};
    AttachNarrative(&game, &progress);
    if (!game.narrativeEnabled || game.phase != PHASE_STORY || game.story.kind != STORY_INTRO
        || game.story.fragment != 0) return Fail("fresh narrative must begin with ROGUE's encounter");
    AdvanceStory(&game);
    if (game.phase != PHASE_NAME_ENTRY) return Fail("first encounter must ask for the player's name");
    GameState before = game;
    const wchar_t* invalidNames[] = {L"", L"   ", L"가\n나", L"A\x202E" L"B", L"abcdefghijklmnopq"};
    for (int i = 0; i < 5; ++i)
        if (SubmitNarrativeName(&game, invalidNames[i]) || memcmp(&game, &before, sizeof(game)))
            return Fail("empty, control, bidi and overlong names must reject without advancing");
    if (!SubmitNarrativeName(&game, L"  서윤  ") || wcscmp(game.narrative.playerName, L"서윤")
        || game.phase != PHASE_STORY || game.story.fragment != 1)
        return Fail("valid name must be trimmed and lead to the training invitation");
    NarrativeProgress named = game.narrative;
    GameState interrupted; NewRun(&interrupted, seed, 0); AttachNarrative(&interrupted, &named);
    if (interrupted.phase != PHASE_STORY || interrupted.story.fragment != 1)
        return Fail("closing after name entry must resume training without asking again");
    AdvanceStory(&game);
    if (game.phase != PHASE_COMBAT || !game.tutorial.active || game.tutorial.step != TUTORIAL_READ)
        return Fail("training invitation must launch an interactive combat tutorial");
    int hp = game.playerHp, enemyHp = game.enemies[0].hp;
    EndTurn(&game); FinishTutorial(&game); AcknowledgeTutorialPreview(&game);
    if (!game.tutorial.active || game.tutorial.step != TUTORIAL_READ || game.playerHp != hp
        || game.enemies[0].hp != enemyHp || AssignDieToSlot(&game, 0, SLOT_ATTACK))
        return Fail("tutorial must require reading dice before placement or execution");
    TutorialReadDice(&game);
    if (game.tutorial.step != TUTORIAL_PLACE || !AssignDieToSlot(&game, 0, SLOT_ATTACK)
        || !AssignDieToSlot(&game, 1, SLOT_DEFEND) || !AssignDieToSlot(&game, 2, SLOT_AMPLIFY)
        || game.tutorial.step != TUTORIAL_PREVIEW)
        return Fail("tutorial must advance only after the three demonstrated placements");
    EndTurn(&game);
    if (game.enemies[0].hp != enemyHp || game.tutorial.step != TUTORIAL_PREVIEW)
        return Fail("tutorial must require acknowledging the actual preview before execution");
    AcknowledgeTutorialPreview(&game);
    if (game.tutorial.step != TUTORIAL_EXECUTE) return Fail("preview acknowledgement must enable execution");
    TurnPreview preview; PreviewTurn(&game, &preview);
    EndTurn(&game);
    if (game.tutorial.step != TUTORIAL_COMPLETE || game.enemies[0].hp >= enemyHp
        || game.playerHp <= 0 || !preview.valid || game.playerHp != hp - preview.damageTaken
        || game.enemies[0].hp != enemyHp - preview.damageDealt || !game.turnTraceCount)
        return Fail("tutorial execution must use the real preview, damage, defense and trace pipeline");
    enemyHp = game.enemies[0].hp; EndTurn(&game);
    if (game.enemies[0].hp != enemyHp) return Fail("completed tutorial must not execute a second combat turn");
    FinishTutorial(&game);
    if (game.tutorial.active || !game.narrative.tutorialSeen || game.narrative.introSeen
        || game.phase != PHASE_STORY || game.story.fragment != 2 || game.selectedDrive != -1
        || game.combatsWon || game.clearedMask || game.playerHp != fresh.playerHp
        || game.rng != fresh.rng || memcmp(game.dice, fresh.dice, sizeof(game.dice))
        || !game.enemyScanned[0])
        return Fail("finishing training must restore the starting deck, RNG and codex without awarding progress");
    named = game.narrative;
    NewRun(&interrupted, seed, 0); AttachNarrative(&interrupted, &named);
    if (interrupted.phase != PHASE_STORY || interrupted.story.fragment != 2 || interrupted.tutorial.active)
        return Fail("closing after training must resume the unread departure card");
    AdvanceStory(&game);
    if (!game.narrative.introSeen || game.phase != PHASE_DRIVE_SELECT)
        return Fail("departure must finish the introduction before volume selection");
    NewRun(&interrupted, seed, 0); AttachNarrative(&interrupted, &game.narrative);
    if (interrupted.phase != PHASE_DRIVE_SELECT || interrupted.tutorial.active)
        return Fail("returning players must bypass completed onboarding");

    // Skip is explicit, and still restores the clean run. It is available before
    // any training action so an experienced player cannot become trapped here.
    NewRun(&game, seed, 0); AttachNarrative(&game, &progress); AdvanceStory(&game);
    SubmitNarrativeName(&game, L"열여섯글자abcdefghi"); AdvanceStory(&game);
    SkipTutorial(&game);
    if (game.tutorial.active || !game.narrative.tutorialSeen || game.phase != PHASE_STORY
        || game.story.fragment != 2 || game.rng != fresh.rng || memcmp(game.dice, fresh.dice, sizeof(game.dice)))
        return Fail("explicit tutorial skip must restore clean play and continue the story");
    printf("PASS: narrative onboarding, name validation, real tutorial actions/preview, skip and interruption recovery\n");
    return 0;
}

static int TestNarrativeMilestoneResume() {
    // Existing saves and interruption checkpoints can have any subset unread.
    // Check all 64 volume masks against every possible relationship-read mask.
    for (int mask = 0; mask < 64; ++mask) for (int seen = 0; seen < 64; ++seen) {
        NarrativeProgress progress = NarrativeReady();
        int recovered = RecoveredShardCount((uint8_t)mask);
        progress.milestoneSeen = (uint8_t)(seen & ((1u << recovered) - 1u));
        progress.bossSeen = 0x1FFFFFu;
        progress.shardSeen = (uint8_t)mask;
        uint8_t expected = progress.milestoneSeen;
        GameState game; NewRun(&game, 0xA703020u, (uint8_t)mask); AttachNarrative(&game, &progress);
        for (int milestone = 0; milestone < recovered; ++milestone) {
            if (expected & (1u << milestone)) continue;
            if (game.phase != PHASE_STORY || game.story.kind != STORY_MILESTONE
                || game.story.fragment != milestone || game.story.page != 0)
                return Fail("resume must deliver unread relationship scenes in recovery-count order");
            int pages = milestone == 5 ? 3 : 1;
            if (StoryPageCount(&game) != pages) return Fail("the sixth revelation must retain all three pages");
            for (int page = 0; page < pages; ++page) {
                const StoryFragment* story = CurrentStoryFragment(&game);
                if (!story || !story->line5 || game.story.page != page || game.narrative.milestoneSeen != expected)
                    return Fail("a partly read conversation must stay pending until its final page");
                AdvanceStory(&game);
            }
            expected |= (uint8_t)(1u << milestone);
            if (game.narrative.milestoneSeen != expected) return Fail("only the completed conversation may be marked read");
        }
        if (game.phase != PHASE_DRIVE_SELECT || game.narrative.milestoneSeen != ((1u << recovered) - 1u)
            || game.clearedMask != mask) return Fail("resumed conversations must preserve clears and return to volume selection");
    }
    NarrativeProgress progress = NarrativeReady(); progress.milestoneSeen = 0x1F;
    progress.bossSeen = 0x1FFFFFu; progress.shardSeen = 0x3F;
    GameState game; NewRun(&game, 1u, 0x3F); AttachNarrative(&game, &progress); AdvanceStory(&game);
    CampaignState saved; InitCampaign(&saved); RecordCampaignClears(&saved, game.clearedMask);
    RecordCampaignNarrative(&saved, &game.narrative);
    CampaignTestFile file;
    if (!SaveCampaign(&saved, file.path) || !LoadCampaign(&saved, file.path)) return Fail("partial sixth revelation checkpoint");
    NewRun(&game, 2u, CampaignClearedMask(&saved)); AttachNarrative(&game, &saved.narrative);
    if (game.story.kind != STORY_MILESTONE || game.story.fragment != 5 || game.story.page != 0
        || game.narrative.milestoneSeen != 0x1F) return Fail("interrupted sixth revelation must restart fully, never disappear");

    // The combat clear is saved before its evidence is read. Closing at either
    // the final boss record or the shard must recover that evidence, with no
    // hidden remount, reward, or lost relationship scene.
    for (int afterBoss = 0; afterBoss < 2; ++afterBoss) {
        InitCampaign(&saved); RecordCampaignClears(&saved, 0x04);
        progress = NarrativeReady(); progress.bossSeen = (afterBoss ? 7u : 3u) << 6;
        RecordCampaignNarrative(&saved, &progress);
        if (!SaveCampaign(&saved, file.path) || !LoadCampaign(&saved, file.path))
            return Fail("unread recovered-volume evidence checkpoint");
        NewRun(&game, 3u, CampaignClearedMask(&saved)); AttachNarrative(&game, &saved.narrative);
        if (!afterBoss) {
            if (game.story.kind != STORY_BOSS || game.story.fragment != 2 || game.story.drive != 2
                || !game.story.resume || game.selectedDrive != -1 || !CurrentStoryFragment(&game))
                return Fail("closing before the final boss record must recover that drive's evidence");
            AdvanceStory(&game);
        }
        if (game.story.kind != STORY_SHARD || game.story.drive != 2 || !game.story.resume
            || game.narrative.shardSeen || !CurrentStoryFragment(&game))
            return Fail("closing before shard acknowledgement must keep it pending");
        AdvanceStory(&game);
        if (game.narrative.shardSeen != 0x04 || game.story.kind != STORY_MILESTONE || game.story.fragment != 0)
            return Fail("recovered evidence must lead to its pending relationship scene");
        AdvanceStory(&game);
        if (game.phase != PHASE_DRIVE_SELECT || game.narrative.milestoneSeen != 1 || game.selectedDrive != -1
            || game.combatsWon || game.clearedMask != 0x04)
            return Fail("evidence recovery must not remount a volume or award another victory");
    }
    printf("PASS: 4096 nonlinear milestone resumes and interrupted three-page revelation persistence\n");
    return 0;
}

static int NarrativeAdvanceRun(GameState* game, int* cards, int* battles, int* milestones, int* shards) {
    if (game->phase == PHASE_COMBAT) { DebugWinCombat(game); ++*battles; return 0; }
    if (game->phase == PHASE_DIRECTORY) {
        int choice = -1;
        for (int i = 0; i < game->directory.choiceCount; ++i)
            if (game->directory.choices[i].kind != DIR_NODE_LOGS) { choice = i; break; }
        if (choice < 0) return Fail("mandatory-story route must not require optional LOGS");
        SelectDirectoryChoice(game, choice); return 0;
    }
    if (game->phase == PHASE_REWARD) { SkipReward(game); return 0; }
    if (game->phase == PHASE_PRUNE) {
        while (UsedBytes(game) > EffectiveCapacity(game)) {
            int index = MostExpensiveFace(game); if (index < 0) return Fail("narrative campaign prune fixture");
            PruneFace(game, index / 6, index % 6);
        }
        ConfirmPrune(game); return 0;
    }
    if (game->phase == PHASE_STORY) {
        const StoryFragment* story = CurrentStoryFragment(game);
        if (!story || !story->line1 || !story->line5) return Fail("every mandatory narrative page must have its complete script");
        if (game->story.kind == STORY_LOGS) return Fail("optional scenes must not enter the mandatory route");
        if (game->selectedDrive != DRIVE_FINAL && (game->story.kind == STORY_TRUTH || game->story.kind == STORY_A_GREETING))
            return Fail("the final confrontation must stay behind the six-volume gate");
        if (game->story.kind == STORY_MILESTONE) ++*milestones;
        if (game->story.kind == STORY_SHARD) ++*shards;
        if (game->story.kind == STORY_A_GREETING && (game->floor != 2 || game->encounter != 2))
            return Fail("welcome reprise must occur at the actual final boss encounter");
        ++*cards; AdvanceStory(game); return 0;
    }
    return Fail("unexpected narrative campaign phase");
}

static int TestNarrativeCampaign() {
    // Choose only cards actually offered by the game. Different offered-card
    // indices exercise nonlinear play without inventing an unavailable volume.
    for (int order = 0; order < 6; ++order) {
        CampaignState campaign; InitCampaign(&campaign);
        GameState game; NewRun(&game, 0xA703100u + order, 0); AttachNarrative(&game, &campaign.narrative);
        int cards = 0, battles = 0, milestones = 0, shards = 0;
        ++cards; AdvanceStory(&game); SubmitNarrativeName(&game, L"서윤");
        ++cards; AdvanceStory(&game); SkipTutorial(&game);
        ++cards; AdvanceStory(&game);
        RecordCampaignNarrative(&campaign, &game.narrative);
        CampaignTestFile file;
        for (int volume = 0; volume < 7; ++volume) {
            if (volume) {
                NewRun(&game, 0xA703100u + order + volume, CampaignClearedMask(&campaign));
                AttachNarrative(&game, &campaign.narrative);
            }
            if (game.phase != PHASE_DRIVE_SELECT) return Fail("completed onboarding and milestones must lead directly to mounting");
            int choice = (order + volume) % game.driveChoiceCount;
            int selected = game.driveChoices[choice];
            if ((volume < 6 && selected == DRIVE_FINAL) || (volume == 6 && selected != DRIVE_FINAL))
                return Fail("final volume must unlock exactly after the six distinct regular volumes");
            SelectDrive(&game, choice);
            int guard = 0, beforeBattles = battles;
            while (game.phase != PHASE_CHAPTER_CLEAR && game.phase != PHASE_ENDING_CHOICE && guard++ < 100) {
                if (NarrativeAdvanceRun(&game, &cards, &battles, &milestones, &shards)) return 1;
                RecordCampaignClears(&campaign, game.clearedMask);
                RecordCampaignNarrative(&campaign, &game.narrative);
            }
            if (guard >= 100 || battles - beforeBattles != 9) return Fail("narrative must preserve each volume's nine-fight flow");
            if (volume < 6 && (game.phase != PHASE_CHAPTER_CLEAR || RecoveredShardCount(game.clearedMask) != volume + 1
                || game.narrative.milestoneSeen != ((1u << (volume + 1)) - 1u)))
                return Fail("each new clear must finish its relationship conversation exactly once");
            if (!SaveCampaign(&campaign, file.path) || !LoadCampaign(&campaign, file.path)) return Fail("between-volume narrative checkpoint");
        }
        if (cards != 41 || battles != 63 || milestones != 8 || shards != 6
            || game.narrative.bossSeen != 0x1FFFFFu || game.narrative.logsSeen || !game.finalVolumeCleared)
            return Fail("first campaign must deliver 41 pre-ending cards, 63 fights, 21 boss records and six distinct recoveries");
        for (int ending = 0; ending < ENDING_COUNT; ++ending) {
            GameState endingGame = game;
            SelectEnding(&endingGame, ending);
            const StoryFragment* first = CurrentStoryFragment(&endingGame);
            if (endingGame.phase != PHASE_STORY || StoryPageCount(&endingGame) != 2 || CommittedEnding(&endingGame) != ending)
                return Fail("every command must commit only its own two-page ending");
            AdvanceStory(&endingGame);
            const StoryFragment* second = CurrentStoryFragment(&endingGame);
            if (endingGame.phase != PHASE_STORY || endingGame.story.page != 1 || !second
                || !wcscmp(first->line1, second->line1)) return Fail("ending page two must be visible before victory");
            AdvanceStory(&endingGame);
            if (endingGame.phase != PHASE_VICTORY || CommittedEnding(&endingGame) != ending
                || endingGame.clearedMask != 0x3F || wcscmp(endingGame.narrative.playerName, L"서윤"))
                return Fail("43-card ending must preserve the name and progress, including RESTORE");
        }
        // After all six, the ordinary replay page permits another completed
        // volume. It may replay boss records, but never absorb a seventh shard.
        NewRun(&game, 0xA703900u, 0x3F); AttachNarrative(&game, &campaign.narrative);
        SetReplayDrivePage(&game, 0);
        SelectDrive(&game, 1);
        int replayCards = 0, replayBattles = 0, replayMilestones = 0, replayShards = 0, guard = 0;
        while (game.phase != PHASE_CHAPTER_CLEAR && guard++ < 100) {
            if (game.phase == PHASE_STORY && game.story.kind == STORY_BOSS && !game.story.replay)
                return Fail("seen boss records must be labelled as replays");
            if (NarrativeAdvanceRun(&game, &replayCards, &replayBattles, &replayMilestones, &replayShards)) return 1;
        }
        if (guard >= 100 || replayBattles != 9 || replayMilestones || replayShards
            || game.clearedMask != 0x3F || game.narrative.milestoneSeen != 0x3F)
            return Fail("duplicate volume clears must never deepen corruption or award another relationship scene");
    }
    printf("PASS: six nonlinear narrative campaigns, 43 mandatory cards, 63 fights, three two-page endings and duplicate-clear replay\n");
    return 0;
}

static int TestNarrativeOptionalRecords() {
    NarrativeProgress progress = NarrativeReady(); progress.milestoneSeen = 0x3F;
    progress.bossSeen = 0x1FFFFFu; progress.shardSeen = 0x3F;
    for (int drive = 0; drive < DRIVE_COUNT; ++drive) for (int floor = 0; floor < 3; ++floor) {
        uint32_t bit = 1u << (drive * 3 + floor);
        GameState game; NewRun(&game, 0xA703A00u + drive * 3 + floor, 0x3F);
        AttachNarrative(&game, &progress);
        game.driveChoices[0] = drive; SelectDrive(&game, 0);
        if (game.phase == PHASE_STORY) AdvanceStory(&game); // A entry recording.
        game.floor = floor; game.encounter = 0;
        game.directory.choiceCount = 1; game.directory.choices[0].kind = DIR_NODE_LOGS;
        uint8_t scanned[ENEMY_KIND_COUNT]; memcpy(scanned, game.enemyScanned, sizeof(scanned));
        SelectDirectoryChoice(&game, 0);
        if (game.phase != PHASE_STORY || game.story.kind != STORY_LOGS || game.story.fragment != floor
            || game.story.replay || (game.narrative.logsSeen & bit)) return Fail("each floor's optional record must start unread");
        const StoryFragment* story = CurrentStoryFragment(&game);
        if (!story || !story->line1 || !story->line4) return Fail("all 21 optional records must have their own complete text");
        AdvanceStory(&game);
        if (game.phase != PHASE_COMBAT || !(game.narrative.logsSeen & bit)
            || !DirectoryIntelActive(&game) || game.enemies[0].block != DIR_LOGS_BLOCK
            || memcmp(scanned, game.enemyScanned, sizeof(scanned)) || game.narrative.milestoneSeen != 0x3F)
            return Fail("reading optional records must retain intel cost, permanent codex and mandatory progression");
        BeginStory(&game, STORY_LOGS, floor, PHASE_COMBAT);
        if (!game.story.replay) return Fail("optional rereads must be identified as historical records");
        AdvanceStory(&game); progress = game.narrative;
    }
    if (progress.logsSeen != 0x1FFFFFu) return Fail("all 21 optional discoveries must occupy distinct persistent bits");
    printf("PASS: all 21 optional LOGS records, read/replay flags, intel cost and codex independence\n");
    return 0;
}
