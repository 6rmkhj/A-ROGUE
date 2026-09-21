# Graph Report - A-ROGUE  (2026-09-21)

## Corpus Check
- 72 files · ~239,376 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 19 file(s) not represented in the graph (top: .bat 8, .inl 4, .tsv 3)

## Summary
- 1676 nodes · 6807 edges · 70 communities (68 shown, 2 thin omitted)
- Extraction: 66% EXTRACTED · 34% INFERRED · 0% AMBIGUOUS · INFERRED: 2298 edges (avg confidence: 0.85)
- Token cost: 484,324 input · 0 output

## Community Hubs (Navigation)
- Game State & Run Flow
- GDI Render Primitives
- Scene FX Easing & Tracks
- Color Mixing & Boss FX
- Text Panels & End Screens
- Main Loop & Ambient FX
- Smoke Test Harness
- Window Input & SFX Dispatch
- Click Hit-testing & Settings UI
- Boot Intro Camera Scene
- Combat Screen Drawing
- Story Rebuild & Endings Docs
- Boot Film Frame Checks
- Campaign Save/Load
- Boss Gimmick Families
- Balance & Reward Curves
- Drive Media Descent Art
- Story Plan & Header Map
- Audio Device & SFX Render
- Attack & Damage Resolution
- Rulebook Drives & Damage
- FX Check Harness
- Directory View Drawing
- Blender Boot Motion Scripts
- Code Map & FX Plan
- Chiptune Music Synth
- Music Scene Sync & WAV
- Narrative Generation Pipeline
- Mob Traits & Gimmick Plan
- FX Check DC State
- Story Line UI Timing
- Localization & Translations
- Cinematic Production Record
- Story Cast & Endings
- Enemy Planning & Scan
- Presentation Reset & Preview
- Story & Tutorial Beats
- UI Focus State
- Icon & Bundle Tools
- Music Song Struct
- Music Rework Plan
- Curve Sim Stats
- Campaign Structure Plan
- Play Review & Triage
- Audio Cue Checks
- Narrative Check & Polish
- SFX Spec Struct
- Multi-enemy Spawn Plan
- FX Timing Trace
- Turn Trace & Combat FX
- Chain Slot Redesign
- Boot Film Packing
- Presentation Polish Review
- Preview Canvas
- Settings Capture & Volume
- Settings Apply
- Drive Media Mount Beats
- A:\ROGUE Final Volume Story
- Directory Choice & RNG
- Death Rot Effect
- Wave Stats
- Music Data Generator
- Campaign Test Fixture
- Mix Voice Struct
- Boot Cue
- Boss Cue
- Mount Cue
- Title Cue
- Enemy Action Display
- FX Check Tick

## God Nodes (most connected - your core abstractions)
1. `MakeRect()` - 201 edges
2. `Fill()` - 127 edges
3. `MixColor()` - 115 edges
4. `TextRect()` - 77 edges
5. `main()` - 74 edges
6. `FxScale()` - 65 edges
7. `HandleClick()` - 65 edges
8. `main()` - 65 edges
9. `FxDecorOn()` - 60 edges
10. `DrawGimmickFx()` - 59 edges

## Surprising Connections (you probably didn't know these)
- `SubmitNarrativeName()` --implements--> `이름 입력 — 표시 이름 변경`  [INFERRED]
  src/game.cpp → docs/A_ROGUE_스토리_기획.md
- `wav.exe 감상·구조 로그 도구` --references--> `MusicRender()`  [EXTRACTED]
  MUSIC_PLAN.md → src/music.cpp
- `MixColor 0-100 Clamp Fix` --references--> `MixColor()`  [EXTRACTED]
  PRESENTATION_REVIEW.md → src/render.cpp
- `drive 0~4 승률 불변 안전망` --semantically_similar_to--> `NewRun clearedMask 인자 주입`  [INFERRED] [semantically similar]
  SPAWN_PLAN.md → CAMPAIGN_PLAN.md
- `안 B: 관통 — 연쇄 눈만큼 방어도 무시 피해` --semantically_similar_to--> `오염(관통) 의도 — 방어도 절반 적용`  [INFERRED] [semantically similar]
  CHAIN_PLAN.md → RULEBOOK.md

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **드라이브 테마 기믹 계열 6종** — boss_gimmicks_fam_lock, boss_gimmicks_fam_restore, boss_gimmicks_fam_offline, boss_gimmicks_fam_route, boss_gimmicks_fam_pressure, boss_gimmicks_fam_quarantine, boss_gimmicks_gimmickfamily [EXTRACTED 1.00]
- **A:\ROGUE 최종 보스의 계열 차용** — boss_gimmicks_signature, boss_gimmicks_seventeenth, boss_gimmicks_last_write, boss_gimmicks_fam_lock, boss_gimmicks_fam_restore, boss_gimmicks_fam_quarantine, campaign_plan_final_boss_family_borrowing [EXTRACTED 1.00]
- **결정론·회귀 방지 장치** — code_map_elapsed_ms_pure_frame, fx_plan_integer_only, fx_plan_balance_invariant, campaign_plan_clearedmask_injection, music_plan_music_rng_isolation, spawn_plan_drive_isolation_safety_net [INFERRED 0.85]
- **Supporting Cast Grounding the Five Endings** — story_rebuild_guard, story_rebuild_naru, story_rebuild_relay_service, story_rebuild_mongnok, story_rebuild_seventeen, story_rebuild_five_endings [INFERRED 0.85]
- **Code-based Boot Intro Overhaul** — presentation_review_boot_intro_camera, presentation_review_boot_intro_sfx, presentation_review_boot_intro_music, presentation_review_temperature_axis, presentation_review_18_sector_motif, presentation_review_boot_flip_at_sync [EXTRACTED 1.00]
- **Roguelike Comparison Gaps** — review_p1_failure_rewards, review_p2_dead_chain_slot, review_p3_uniform_turns, review_hades, review_dicey_dungeons, review_slay_the_spire [EXTRACTED 1.00]
- **로그 대본 → 서사 데이터·상태 → 검증 적용 파이프라인** — docs_a_rogue_스토리_기획, docs_a_rogue_선택기록_대본, tools_generate_narrative, src_narrative_data, src_narrative_state, tools_narrative_check, docs_story_implementation [INFERRED 0.85]
- **여섯 볼륨 복구가 로그 침식과 원본 반전으로 수렴하는 구조** — docs_a_rogue_스토리_기획_six_verification_shards, docs_a_rogue_스토리_기획_erosion, docs_a_rogue_스토리_기획_recovery_count_dialogue, docs_a_rogue_스토리_기획_rogue, docs_a_rogue_스토리_기획_protagonist [EXTRACTED 1.00]
- **연출 개선 오프스크린 검증 도구 묶음** — tools_fx_check, tools_narrative_check, tools_audio_check, tools_boot_film_check, docs_presentation_polish_offscreen_verification [INFERRED 0.95]

## Communities (70 total, 2 thin omitted)

### Community 0 - "Game State & Run Flow"
Cohesion: 0.07
Nodes (103): BossGimmickInfo, BossRuntime, AccessDeniedSlot(), AcknowledgeTutorialPreview(), AddEnemy(), AnnounceQuarantineTarget(), ApplyDirectoryChoice(), ApplyDirectoryCombatSetup() (+95 more)

### Community 1 - "GDI Render Primitives"
Cohesion: 0.08
Nodes (65): FxAlphaBlendProc, HBRUSH, #76 Aspect Ratio Letterbox (1120x760 canvas), AcquireSpinScratch(), AppendStatus(), Bar(), BuildOneMip(), ComputeCanvasTransform() (+57 more)

### Community 2 - "Scene FX Easing & Tracks"
Cohesion: 0.14
Nodes (61): CombatLancePoint(), CombatLanceProgress(), DrawBossHalo(), DrawDiePips(), DrawEnergyLance(), DrawFracture(), DrawImpactCut(), DrawProcessStage() (+53 more)

### Community 3 - "Color Mixing & Boss FX"
Cohesion: 0.14
Nodes (65): GimmickFxA(), CorruptCode(), DrawBandGlitch(), DrawScreenStatic(), DrawSignalPath(), Fill(), Hash3(), MixColor() (+57 more)

### Community 4 - "Text Panels & End Screens"
Cohesion: 0.10
Nodes (63): DifficultyInfo, DifficultyInfoOrNull(), Face, UINT, DrawScanlines(), DrawSectorStatic(), FaceColor(), FormatFace() (+55 more)

### Community 5 - "Main Loop & Ambient FX"
Cohesion: 0.06
Nodes (60): imm, AmbientNoiseBand(), AmbientNoiseLevel(), AmbientNoisePulse(), AmbientNoiseSurge(), AmbientSlip(), ArmOrTakeDirectory(), BeginDirectoryEnter() (+52 more)

### Community 6 - "Smoke Test Harness"
Cohesion: 0.15
Nodes (60): AssignDieToSlot(), CommittedEnding(), ConfigureDriveForTest(), CurrentStoryFragment(), EndTurn(), MixDirectoryDrive(), NewRun(), PreviewTurn() (+52 more)

### Community 7 - "Window Input & SFX Dispatch"
Cohesion: 0.08
Nodes (53): LPARAM, LRESULT, PlaySfx(), PlaySfxPitched(), TutorialExecuteStep(), ArmOrConfirmRewardSkip(), ArmOrInstallRewardOnFace(), ArmOrTakeTsrReward() (+45 more)

### Community 8 - "Click Hit-testing & Settings UI"
Cohesion: 0.11
Nodes (53): CanUndoPrunedFace(), InstalledTsrAt(), InstalledTsrCount(), ActivateKeyboardFocus(), ClearStaleConfirmations(), ClickPrune(), RECT, HandleClick() (+45 more)

### Community 9 - "Boot Intro Camera Scene"
Cohesion: 0.11
Nodes (52): src_narrative_visuals, AcquireBuffer(), BootApply(), BootArc(), BootBox(), BootCam, at, pitch (+44 more)

### Community 10 - "Combat Screen Drawing"
Cohesion: 0.08
Nodes (49): DieState, CombatFxElapsed(), CombatFxPlaying(), DisplayDie(), EnemyBob(), EnemyStrikeDamage(), EnemyStrikePop(), GimmickFxB() (+41 more)

### Community 11 - "Story Rebuild & Endings Docs"
Cohesion: 0.07
Nodes (49): 기록 카드 형식 (제목·경로·도장·본문 5줄), GDI-only Procedural Visuals (no added asset files), A:\ROGUE README, ARVF v1 Frame Pack Format, Higgsfield 6.32s New-game Cinematic, RESTORE / HOLD / EXIT Final Choice, 21 Optional LOGS Records, Play Flow (Read, Place, Preview, Execute) (+41 more)

### Community 12 - "Boot Film Frame Checks"
Cohesion: 0.07
Nodes (41): IWICImagingFactory, LARGE_INTEGER, limits, new, BYTE, HDC, DecodeFilmFrame(), DestroyBootFilm() (+33 more)

### Community 13 - "Campaign Save/Load"
Cohesion: 0.14
Nodes (41): CampaignState, HINSTANCE, PWSTR, Campaign Save v3 (AROGUE.SAV, 72-byte, checksum), BesideExecutable(), CampaignClearedMask(), CampaignSeenEndingMask(), ChecksumN() (+33 more)

### Community 14 - "Boss Gimmick Families"
Cohesion: 0.12
Nodes (39): BOSS_GIMMICKS.md - 드라이브별 보스 기믹 명세서 v2.0, ACCESS.DENIED (C:\ 1층 보스, 짝수 턴 슬롯 잠금), AUTOPLAY (E:\ 1층 보스, 3턴마다 주사위 오프라인), BLUE.SCREEN 파쇄 (C:\ 3층 보스), BossGimmickKind (21개 고유 보스 행동), BossRuntime (판당 하나인 보스 기믹 런타임 상태), 한 턴 N+ 피해 임계 대응 패턴 (지연·회피·게이지 감소), FAM_LOCK - 슬롯 권한 잠금 계열 (C:\) (+31 more)

### Community 15 - "Balance & Reward Curves"
Cohesion: 0.14
Nodes (32): AssignDice(), AverageFacePower(), BestReward(), ChooseDirectory(), GameState, DirectoryScore(), EffectiveDiePower(), main() (+24 more)

### Community 16 - "Drive Media Descent Art"
Cohesion: 0.12
Nodes (31): ModifierInfo, DrawGlowRing(), ActiveModifierInfo(), MountBeats, DrawDamageRun(), DrawDescent(), DrawMediaBody(), DrawMediaCage() (+23 more)

### Community 17 - "Story Plan & Header Map"
Cohesion: 0.11
Nodes (20): A:\ROGUE 선택 기록 대본 (LOGS 21편), A-2 전술이 아닌 답 (PREFERENCE.TXT), LOGS 선택 기록 배치·읽기 규칙, 본편과의 연결 확인 표, A:\ROGUE 스토리 기획 — 로그와 마지막 복구, 과거 반복 — 인격 백업 계보 vs 현재 재도전, 중심 질문 — 처음의 나로 돌아가기 위해 지금의 우리를 지워도 되는가, 종료 잠금 — 보호 식별자와 두 복구 조건 (+12 more)

### Community 18 - "Audio Device & SFX Render"
Cohesion: 0.11
Nodes (22): mmsystem, AddRoomTap(), AudioClose(), AudioGuard, AudioOpen(), AudioThread(), AudioUnderruns(), BootNoteHz() (+14 more)

### Community 19 - "Attack & Damage Resolution"
Cohesion: 0.17
Nodes (27): ChainTarget(), CombatFxEvent, DamageEnemy(), DieForSlot(), DriveRulePrepareResolve(), EffectiveLawDrive(), FirstLivingEnemy(), HitFxFlags() (+19 more)

### Community 20 - "Rulebook Drives & Damage"
Cohesion: 0.14
Nodes (24): 남은 볼륨 3개 미만 시 같은 볼륨·다른 난이도 3장 제시, 음악 전용 noiseRng — 게임 RNG 불간섭 규약, RULEBOOK.md - A:\ROGUE 규칙서 v2.0, 덱과 용량 (18면·층 한도 240/180/130B·정리 화면), 디스크 손상 5종 (배드 섹터·읽기 오류·조각화·과잉 할당·체크섬), C:\ SYSTEM 드라이브 (배드 섹터+체크섬, 최대 체력 +6), D:\ ARCHIVE 드라이브 (배드 섹터+과잉 할당, 용량 +15B), N:\ NETWORK 드라이브 (읽기 오류+체크섬, 적 체력 +35%) (+16 more)

### Community 21 - "FX Check Harness"
Cohesion: 0.25
Nodes (23): DisplayTurn(), FxSnapshotHeld(), DrawHeader(), CheckBootBlendFrames(), CheckBootBlendPixels(), CheckDecorationState(), CheckInteractionFrames(), CheckPresentationSafety() (+15 more)

### Community 22 - "Directory View Drawing"
Cohesion: 0.17
Nodes (24): DirChoiceRow(), DirClamp(), DirDrawDoor(), DirDrawHead(), DirDrawTable(), DirDrawTunnel(), DirFillScale(), DirPath (+16 more)

### Community 23 - "Blender Boot Motion Scripts"
Cohesion: 0.10
Nodes (8): bpy, bpy_extras_object_utils, math, mathutils, Headless geometric regression checks for the exported boot cinematic., path(), Refine the inspected Higgsfield boot scene, without replacing its objects. Run…, Monotone Hermite per coordinate: C1 joins, no waypoint overshoot.

### Community 24 - "Code Map & FX Plan"
Cohesion: 0.18
Nodes (23): CODE_MAP.md - 코드 읽기 지도 (Unity 이전 준비), tools/bundle.py 읽기 전용 번들 생성기 (build/read/), tools/check-fx.bat 픽셀 재현성 검사, 프레임 = 경과 ms의 순수 함수 규약, 고정 캔버스 1120×760 후 창 크기로 확대, PaintGame 프레임 경로 (phase 분기·단일 오버레이), main.cpp 전역 상태 소유 (gGame·gCampaign·gSettings·연출 플래그), 세 계층 읽기 번들 (01_rules·02_view·03_platform·04_tools) (+15 more)

### Community 25 - "Chiptune Music Synth"
Cohesion: 0.16
Nodes (21): MusicChannelState, Bit(), ChordTone(), Clamp16(), Envelope(), LowPass(), MusicRender(), NoteHz() (+13 more)

### Community 26 - "Music Scene Sync & WAV"
Cohesion: 0.17
Nodes (21): AudioSetCritical(), AudioSetDrive(), AudioSetEnding(), AudioSetIntensity(), AudioSetScene(), SyncAudioScene(), MusicState, MusicInit() (+13 more)

### Community 27 - "Narrative Generation Pipeline"
Cohesion: 0.14
Nodes (13): argparse, json, pathlib, re, Narrative Generation Pipeline, Check the new narrative's authored localization and source text hygiene., Render sparse motion-review frames from the locally exported Higgsfield scene., card() (+5 more)

### Community 28 - "Mob Traits & Gimmick Plan"
Cohesion: 0.22
Nodes (20): GIMMICK_PLAN.md - 몹 기믹 신설·보스 기믹 강화 설계, AUTORUN.INF 자동 실행 (E:\, 1턴 방어도 절반), CheckMobTraits 스모크 시험, [맞붙음] clash — 내 눈과 적 숫자를 직접 비교, 이해가 곧 무장 (E.G.O.형 조건 충족 보상), FALSE.COPY 거짓 사본 (A:\, 같은 눈이면 2배 피해), LOST.CLUSTER 유실 (E:\, 처치 턴 연쇄 채우면 격리 면제), 몹 특성 21종 (EnemyState.counter·EnemyInfo.trait·TraitInfo) (+12 more)

### Community 29 - "FX Check DC State"
Cohesion: 0.10
Nodes (20): SIZE, CheckDcState, bitmap, bk, bkMode, brush, dcBrush, font (+12 more)

### Community 30 - "Story Line UI Timing"
Cohesion: 0.15
Nodes (20): AdvanceStoryLineUi(), AdvanceStoryUi(), BeginBossIntro(), QueueRogueBark(), RogueBarkElapsed(), RogueCallTalks(), StopRead(), StoryLineElapsed() (+12 more)

### Community 31 - "Localization & Translations"
Cohesion: 0.22
Nodes (18): wchar_t, ExpandFormatted(), IsFormatStart(), LoadTranslations(), LocalizeText(), MatchFormatted(), NextLiteral(), SetUiLanguage() (+10 more)

### Community 32 - "Cinematic Production Record"
Cohesion: 0.15
Nodes (17): New-game Cinematic Production Record, Blender 5.1 Emission Socket Rebind, Title/Arrival/Skip Dissolves (490ms, 460ms, 264ms), Higgsfield 3D Jutsu Scene (revision 6), Native Composition Preview (render-boot-preview), Natural-motion Revision, Cinematic Rebuild Pipeline (Blender, ffmpeg, pack, checks), 18-sector Motif (+9 more)

### Community 33 - "Story Cast & Endings"
Cohesion: 0.13
Nodes (18): 복구와 침식 — 잔여 충돌의 ROGUE 이전, 로그·기록·LOGS 표기 구분, 주인공 — 인격 복구 장치의 제작자, 로그(ROGUE) — 플레이어 인격으로 만든 백업 조력자, 여섯 검증 조각 (C·D·E·N·R·X-SHARD), 효과 강도 FULL·REDUCED·OFF, 결과 우선 가독성 원칙, FIN-02 용량표 (외부 장치 512 U) (+10 more)

### Community 34 - "Enemy Planning & Scan"
Cohesion: 0.18
Nodes (17): EnemyInfo, BossStolenPlan(), CorruptPercent(), EnemyState, GetEnemyInfoOrUnknown(), IsEnemyScanned(), IsValidEnemyKind(), MobTraitPlan() (+9 more)

### Community 35 - "Presentation Reset & Preview"
Cohesion: 0.24
Nodes (15): InitTitle(), SetSeenEndings(), FinishBootIntro(), VisibleSceneKey(), CreateRenderFonts(), DestroyRenderFonts(), FxSnapshotCapture(), FxSnapshotDestroy() (+7 more)

### Community 36 - "Story & Tutorial Beats"
Cohesion: 0.22
Nodes (16): 긴 통합 장면의 연속 카드 처리, GamePhase, AdvanceStory(), AttachNarrative(), BeginPendingMilestone(), BeginRecoveredEvidence(), BeginStory(), BeginTutorial() (+8 more)

### Community 37 - "UI Focus State"
Cohesion: 0.18
Nodes (16): DWORD, MoveKeyboardFocus(), SyncUiFocus(), TickUiFocus(), UiFocusAge(), UiFocusCueDue(), UiFocusElapsed(), UiFocusState (+8 more)

### Community 38 - "Icon & Bundle Tools"
Cohesion: 0.18
Nodes (11): collections, os, sys, main(), 소스 전체를 파일 하나로 합친다. 읽기 전용. python tools/bundle.py 결과: build/read/abs.cpp 의존 순서대로…, ico_image(), main(), pixels() (+3 more)

### Community 39 - "Music Song Struct"
Cohesion: 0.13
Nodes (15): MusicSong, arp, bass, bpm, cut, drop, duty, hat (+7 more)

### Community 40 - "Music Rework Plan"
Cohesion: 0.22
Nodes (14): MUSIC_PLAN.md - BGM 재작업 계획, 부품3: 4/8마디 화성 진행 (CHORD 표·A/B 음형), 드라이브별 6곡 성격 (X:\ 15스텝 엇박 유지), 부품5: intensity 층 편곡 (악기가 들어온다), 부품4: 1극 저역통과 필터 (audio.cpp와 같은 식), CheckDriveRulesStoryAndMusic 음악 불변식 7항목, 8비트 NES 칩 특성 (Steps4 삼각파·1비트 LFSR·60Hz 아르페지오 코드), 부품2: 음 엔벨로프 (MusicChannelState.envelope ADSR) (+6 more)

### Community 41 - "Curve Sim Stats"
Cohesion: 0.14
Nodes (14): Stats, blockSum, damageSum, deaths, hpSum, intentSum, intentTurns, maxHpSum (+6 more)

### Community 42 - "Campaign Structure Plan"
Cohesion: 0.27
Nodes (13): CAMPAIGN_PLAN.md - 캠페인 구조 개편 계획, 캠페인 개편 리스크 (조용한 회귀·배열 초기화 부족·캠페인 길이), CampaignState 구조체 + AROGUE.SAV 세이브 파일, NewRun clearedMask 인자 주입, DRIVE_COUNT 7 / DRIVE_SELECTABLE_COUNT 6 분리, SELF-REFERENCE 볼륨 법칙 (EffectiveLawDrive 리팩터), 6볼륨 순차 복구 → A:\ROGUE 개방 캠페인 구조, STORY_SHARD 조각 확보 기록 + 챕터 클리어 화면 (+5 more)

### Community 43 - "Play Review & Triage"
Cohesion: 0.18
Nodes (13): Play Review, Issue Triage and Roguelike Comparison (2026-09-08), #71/#68 Capacity Shrink 240 to 180 to 130B, Dicey Dungeons, #77 Fragmentation Feels Unfair, Hades, P1: Failed Runs Give Nothing, P2: Chain Slot Unused, P3: Uniform Turn Shape (+5 more)

### Community 44 - "Audio Cue Checks"
Cohesion: 0.45
Nodes (12): RenderSfx(), Check(), CheckAllCues(), CheckCombatCues(), CheckInteriorJoins(), CheckMaterialCues(), CheckShortBuffers(), FillGuard() (+4 more)

### Community 45 - "Narrative Check & Polish"
Cohesion: 0.21
Nodes (11): 한 번의 완주 필수 기록 42장 (+A 최종 인사 카드 = 43장), PRESENTATION_POLISH.md — 연출·효과 개선 (2026-09-20), 전투 효과음 11종 개별 엔벨로프와 우선순위, 재사용 GDI 기본 브러시, Higgsfield 3D Jutsu 프로젝트 (revision 4, 190프레임/30fps), 오프스크린 렌더 검증 (check-fx 7,572·check-narrative 1,868프레임·audio 4,838검사·boot-film 190프레임), 이야기 회귀·UI 입력·렌더 검증 (4,096 관계 장면·필수 43장·63전투), CheckStoryFrame() (+3 more)

### Community 46 - "SFX Spec Struct"
Cohesion: 0.17
Nodes (12): SfxSpec, attackMs, bend, cut, duty, hz, ms, noise (+4 more)

### Community 47 - "Multi-enemy Spawn Plan"
Cohesion: 0.36
Nodes (11): balance.exe 휴리스틱 게이트 (드라이브 × 300시드), SANDBOX.BREACH (X:\ 2층 보스, ESCAPEE 소환), 표시 전용 작업의 밸런스 표본 불변 검사, ESCAPEE 탈주 (X:\ 몹, SANDBOX.BREACH 소환체), 몹은 "성질", 보스는 "사건" 규칙, SPAWN_PLAN.md - 다수 적 전투 구현 계획, drive 0~4 승률 불변 안전망, EnemyState.power 소환체 출력 약화 (45%) (+3 more)

### Community 48 - "FX Timing Trace"
Cohesion: 0.42
Nodes (10): FxEventElapsed(), FxImpactHold(), FxTraceAt(), FxTraceLine(), FxTraceSpan(), CombatFxEvent, GameState, CombatFxLeadElapsed() (+2 more)

### Community 49 - "Turn Trace & Combat FX"
Cohesion: 0.18
Nodes (11): BeginCombatClear(), BeginGimmickFx(), BeginPlayerHit(), wchar_t, FinishTurnTrace(), SyncCombatFx(), TermPrint(), TermRun() (+3 more)

### Community 50 - "Chain Slot Redesign"
Cohesion: 0.38
Nodes (10): CHAIN_PLAN.md - 연쇄 슬롯 개편 계획, AROGUE_THIRD 실험 스위치 (세 번째 주사위 슬롯 고정), 연쇄 진단: 약해서가 아니라 읽히지 않아서, 안 A: 이월(충전) — 연쇄 눈을 다음 턴 공격에 더함 (추천), 안 B: 관통 — 연쇄 눈만큼 방어도 무시 피해, 안 C: 짝 맞추기 — 공격 눈과 같으면 두 배, 연쇄 슬롯 (직전 공격/방어 일부 반복), PACKET CHAIN (N:\ 법칙, 연쇄 한 번 더) (+2 more)

### Community 51 - "Boot Film Packing"
Cohesion: 0.22
Nodes (9): contextlib, hashlib, shutil, struct, subprocess, main(), Pack a Higgsfield-rendered 6.32 second film into native WIC JPEG frames. Usage:…, workspace_frames() (+1 more)

### Community 52 - "Presentation Polish Review"
Cohesion: 0.24
Nodes (10): Play-based Presentation Polish Review (2026-09-12), Automatic Turn Handoff (520ms linger), Dice Pips, Slot Power Bands and Reward Code Glyphs, check-fx Render Verification Matrix (6,848 frames), Pre-execution Turn/Intent Snapshot, Single-enemy 888px Stage, Heavy-hit Slash Light and Kill Cross-slash, Temperature Color Axis (white heat to amber to phosphor) (+2 more)

### Community 53 - "Preview Canvas"
Cohesion: 0.20
Nodes (8): HBITMAP, HDC, HGDIOBJ, PreviewCanvas, bitmap, bits, dc, oldBitmap

### Community 54 - "Settings Capture & Volume"
Cohesion: 0.28
Nodes (9): AudioMusicEnabled(), AudioMusicVolume(), AudioSfxVolume(), AudioVolume(), UiLanguage(), AudioChannelVolume(), CaptureSettings(), PersistSettings() (+1 more)

### Community 55 - "Settings Apply"
Cohesion: 0.31
Nodes (9): AudioSetMusicEnabled(), AudioSetMusicVolume(), AudioSetSfxVolume(), SetAudioVolume(), ApplySettings(), SetAudioChannelVolume(), MusicSetEnabled(), ApplyFullscreen() (+1 more)

### Community 56 - "Drive Media Mount Beats"
Cohesion: 0.25
Nodes (8): DriveMedia(), MountBeats, wchar_t, MediaName(), MediaSpindle(), MountBeatsFor(), MountTrackMs(), ScenePace()

### Community 57 - "A:\ROGUE Final Volume Story"
Cohesion: 0.25
Nodes (8): A-3 남아 있는 경로 (OPEN_PATH.LOG), A:\ROGUE 3층 — ROGUE 인증·반사·마지막 실행 (SIGNATURE·SEVENTEENTH·LAST.WRITE), LAST.WRITE 공격 슬롯 보존, 이름 입력 — 표시 이름 변경, "환영합니다. 마스터." 인사 복선, A:\ROGUE 최종 볼륨 — SIGNATURE·SEVENTEENTH·LAST.WRITE (결정권 싸움), 나루 — 외부 장치 안내, 볼륨 필수 선택과 저장 키 (dec_guard·dec_evidence·dec_transit·dec_relay·dec_runtime·dec_authority)

### Community 58 - "Directory Choice & RNG"
Cohesion: 0.29
Nodes (8): DirectoryChoiceCount(), DirectoryIntelActive(), FloorBossKind(), ScheduledMobKind(), SelectDirectoryChoice(), GameState, DirectoryDetailText(), CheckDirectoryRng()

### Community 59 - "Death Rot Effect"
Cohesion: 0.25
Nodes (8): DeathRot, breakStep, brk, gap, infect, origin, seed, step

### Community 60 - "Wave Stats"
Cohesion: 0.25
Nodes (8): WaveStats, clipped, first, last, mean, peak, rms, tailPeak

### Community 61 - "Music Data Generator"
Cohesion: 0.43
Nodes (6): io, bars(), carr(), main(), song_init(), tokens()

### Community 62 - "Campaign Test Fixture"
Cohesion: 0.33
Nodes (5): CampaignTestFile, path, DWORD, wchar_t, WriteCampaignFixture()

### Community 63 - "Mix Voice Struct"
Cohesion: 0.40
Nodes (5): MixVoice, data, length, position, priority

### Community 64 - "Boot Cue"
Cohesion: 0.50
Nodes (4): BootCue, at, pitch, sfx

### Community 65 - "Boss Cue"
Cohesion: 0.50
Nodes (4): BossCue, at, pitch, sfx

### Community 66 - "Mount Cue"
Cohesion: 0.50
Nodes (4): MountCue, at, pitch, sfx

### Community 67 - "Title Cue"
Cohesion: 0.50
Nodes (4): TitleCue, at, pitch, sfx

## Knowledge Gaps
- **172 isolated node(s):** `hz`, `ms`, `bend`, `wave`, `duty` (+167 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 282 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **2 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `A:\ROGUE README` connect `Story Rebuild & Endings Docs` to `Cinematic Production Record`, `Campaign Save/Load`, `Narrative Check & Polish`, `Boss Gimmick Families`, `Story Plan & Header Map`, `Rulebook Drives & Damage`, `Narrative Generation Pipeline`?**
  _High betweenness centrality (0.090) - this node is a cross-community bridge._
- **Why does `A:\ROGUE 스토리 기획 — 로그와 마지막 복구` connect `Story Plan & Header Map` to `Game State & Run Flow`, `Boot Intro Camera Scene`, `Story Rebuild & Endings Docs`?**
  _High betweenness centrality (0.085) - this node is a cross-community bridge._
- **Why does `STORY_IMPLEMENTATION.md — 로그(ROGUE) 스토리 적용` connect `Story Plan & Header Map` to `Game State & Run Flow`, `Story & Tutorial Beats`, `Smoke Test Harness`, `Story Rebuild & Endings Docs`, `Narrative Check & Polish`?**
  _High betweenness centrality (0.077) - this node is a cross-community bridge._
- **Are the 179 inferred relationships involving `MakeRect()` (e.g. with `DrawDiePips()` and `DrawProcessStage()`) actually correct?**
  _`MakeRect()` has 179 INFERRED edges - model-reasoned connections that need verification._
- **Are the 108 inferred relationships involving `Fill()` (e.g. with `DrawDiePips()` and `DrawProcessStage()`) actually correct?**
  _`Fill()` has 108 INFERRED edges - model-reasoned connections that need verification._
- **Are the 102 inferred relationships involving `MixColor()` (e.g. with `DrawBossHalo()` and `DrawEnergyLance()`) actually correct?**
  _`MixColor()` has 102 INFERRED edges - model-reasoned connections that need verification._
- **What connects `hz`, `ms`, `bend` to the rest of the system?**
  _172 weakly-connected nodes found - possible documentation gaps or missing edges._