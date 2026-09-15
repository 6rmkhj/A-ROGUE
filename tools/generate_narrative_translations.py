"""Reviewed English story localization. Generate a TSV for translations.tsv.

The English cards follow the authored Markdown one line at a time, retaining
speaker and name tokens for the runtime name substitution. Run with --check to
verify the generated fragment. This never edits the shared translation table.
"""
from pathlib import Path
import argparse
import re
from generate_narrative import ROOT, MAIN, OPTIONAL

MANDATORY = r"""
ROGUE: This time...
Me: What was that?
ROGUE: Strange. We have only just met.
ROGUE: You can call me Rogue.
ROGUE: Could you tell me your name first?

ROGUE: If you want out, you need to know how to fight.
ROGUE: I will bring up a practice process.
ROGUE: Nothing here goes on the real record, so take it easy.
ROGUE: If you get stuck, I will walk you through it.
ROGUE: All right. Here we go.

ROGUE: The way out is split across six connections.
ROGUE: We need to restore C, D, E, N, R, and X.
Me: Can you leave, too?
ROGUE: I cannot read my return entry yet.
ROGUE: Let us open the way home first.

SYSTEM: Access denied. Protecting user.
Me: Why attack someone you are protecting?
ROGUE: The order to admit us and the order to stop us...
ROGUE: They carry the same signature.
Me: Who gave those orders?

ROGUE: It flagged your strongest output as an error.
Me: That was the action I chose.
ROGUE: It rejects anything outside its reference state.
Me: What if that reference is wrong?
ROGUE: That judgment is not yet yours to make.

SYSTEM: Conflicting protection policies.
RECORD: If the user changes, restore the reference state.
Me: Without checking whether I changed for the better?
ROGUE: It only checks that you changed.
ROGUE: The policy author's identity is still locked.

SYSTEM connection restored.
Original policy: preserve the reference state.
Termination requires the user's consent.
ROGUE: Permission to run is not permission to stop.
Residual conflicts transfer to ROGUE.

Me: I damaged it. Now it is back.
ROGUE: It did not repair the damage.
ROGUE: It overwrote this moment with an earlier one.
Me: Then what happened to what I just did?
ROGUE: It is in the record. Just not in the present.

Every record begins with the same sentence.
Only the last sentence changes.
Me: Why keep so many copies of the same file?
ROGUE: Perhaps they were kept because they differ.
ROGUE: Please do not erase their endings.

SYSTEM: Restoring the last healthy state.
Me: Who decided that was the last one?
The date marked healthy has never changed.
ROGUE: New experiences never enter the reference.
ROGUE: They all pile up under failures.

ARCHIVE connection restored.
Original policy: archive changes after the reference.
Keeping a record is not accepting it as the present.
ROGUE: Recovery is done, but the failure archive grew.
Residual conflicts transfer to ROGUE.

A photograph opens automatically.
A desk in front of a monitor. A cup still warm.
Me: That is the room I was in before I came here.
ROGUE: The outside is still connected.
ROGUE: You still have somewhere to return to.

Me: Could I get home by disconnecting?
ROGUE: Look at the disconnected die.
ROGUE: Its value remains, but it has no path to execute.
ROGUE: You need to restore your return path, too.
Me: Pulling the plug will not be enough.

External connection confirmed: one user.
Guide entity's external connection: none.
Me: I cannot find anywhere for you to return to.
ROGUE: No. Perhaps it is missing, or just unreadable.
ROGUE: I cannot tell yet.

REMOVABLE connection restored.
One path connects to a physical body outside.
All fragments are needed to verify its owner.
Me: Let us keep looking for your path, too.
Residual conflicts transfer to ROGUE.

UNKNOWN SENDER: Do not send it there.
Me: Are they talking to us?
ROGUE: The recipient labels have been reversed.
ROGUE: I will read the earlier message first.
UNKNOWN SENDER: Someone is still inside.

REQUEST: Help me.
RESPONSE: Do not retransmit the same request.
REQUEST: Help me.
Me: It was waiting for an answer, not malfunctioning.
ROGUE: The receiver only saw a repeated signal.

A message arrives after its deadline.
RECORD: To end this, check the termination conditions.
RECORD: After recovery, my hands would not stop.
Me: Was the person who left this a guide, too?
ROGUE: Restoring its source should tell us.

NETWORK connection restored.
A recovery request is not consent to termination.
Even late messages remained in the inbox.
ROGUE: Being late does not make a message worthless.
Residual conflicts transfer to ROGUE.

ROGUE: The attack was recorded.
ROGUE: The hesitation before it was not.
Me: Should that be recorded, too?
ROGUE: Without it, how would we know why you did it?
A brief note is added beside the attack record.

The job ends, but memory usage does not fall.
Me: What keeps lingering?
ROGUE: States that cannot finish.
ROGUE: Closing one causes another to call it back.
Me: More space alone will not solve this.

The test to archive the current state fails.
Only an old default state can be read again.
ROGUE: Removing the damage removes new memories, too.
Me: That is not the state that was here a moment ago.
ROGUE: That is why I cannot call the save complete.

RAMDISK connection restored.
The template file does not match the running state.
Changes made now require separate verification.
ROGUE: Remembering is not the same as saving.
Residual conflicts transfer to ROGUE.

Specimen classification: disobeys commands.
Me: It does not say why it refused.
ROGUE: There is no field for the reason.
Me: Then let us write it here.
Rogue adds a blank line beside the verdict.

The escapee tried to seize another execution slot.
A different specimen's voice leaked through behind it.
RECORD: Move me and the next place will become this.
Me: Same quarantine tag. Different intentions.
ROGUE: Too different to fit under one name.

Attack code and pleas share the quarantine list.
RECORD: If I disappear, please stop the tests.
Me: That was not a request to be repaired.
ROGUE: The system read it as a request to recreate them.
Me: It should have heard that they wanted to stop.

QUARANTINE connection restored.
Quarantined states retain a protected identity.
Deletion triggers recreation from the same reference.
ROGUE: The preservation order will not let go.
Residual conflicts transfer to ROGUE.

ROGUE: One connection has come back.
Me: Are you all right? Your voice cut out.
ROGUE: Isolating residual errors.
Me: Where?
ROGUE: In me. I am all right for now.

Me: Which face do you like best?
ROGUE: Three.
Me: Not six?
ROGUE: You asked which I like, not which is good.
Me: Right. I will write that down, too.

ROGUE: Recovery status normal. Ready to continue.
Me: I asked how you were.
ROGUE: ...I am not all right.
ROGUE: I thought I was not allowed to say it unasked.
Me: From now on, tell me first.

ROGUE: I remember escaping. I remember staying, too.
ROGUE: Both feel as though I lived them myself.
Me: Let us write them down instead of reliving them.
ROGUE: Will you stop and read them with me?
Me: Yes. Before we go any further.

ROGUE: Moving all of me moved the conflicts, too.
ROGUE: The healthy copy has none of our time together.
Me: I cannot call that you and leave it there.
ROGUE: Once the last fragment opens, check how to stop.
Me: And this time, what you want matters, too.

Six verification fragments point to one signature.
Personality source and policy author: current user.
Guide instance: ROGUE.
ROGUE: ...You were the original.
ROGUE: I am a backup made from your personality.

Test records unfold, each returning to the reference.
Different guides stop at the same ending.
Me: All these memories ended up inside you?
ROGUE: How many of me do you think there have been?
ROGUE: But the me who met you this time is only one.

Administrator handoff. Quarantined states connect.
ROGUE: Now it all arrives as if I am living it.
ROGUE: I want you to get out. I do not want to end.
Me: Rogue. We have not decided how this ends yet.
A:\ROGUE opens. The guide window goes dark.

F1 still opens the guide, but no new reply comes.
Me: Rogue, can you hear me?
SAVED GUIDE: Read the faces. Choose their places.
SAVED GUIDE: Execution is your decision.
I go deeper, following what he taught me.

The user and guide signatures overlap.
SYSTEM: Termination of a protected entity is denied.
Me: Must the same beginning mean the same ending?
ROGUE: ...I also chose things you did not choose.
The authentication lock opens a deeper path.

My last output came back as its next attack.
ROGUE: I know how you fight.
Me: You taught me.
ROGUE: Still, that does not make our next choices equal.
The output following me breaks off for the first time.

Final policy record: refused to delete the attack path.
Me: Why leave this one?
ROGUE: I could not take away your way to end this.
ROGUE: ...But I am still afraid.
The administrator stops. The personality lingers.

Physical user connected. Termination terms editable.
A backup could not give consent on the user's behalf.
Me: I was the one who made it impossible to stop.
ROGUE: This time, choose the ending yourself.
Reference restoration and auto-restart may be revoked.

Me: If we start over, there might be another way.
ROGUE: Will you remember saying that?
This conversation is classified as a failed test.
The reference overwrites the current user state.
Rogue's call window returns to its initial shape.

Welcome, Master.
The guide pauses at the sight of someone familiar.
ROGUE: This time...
SYSTEM: Starting a new recovery.
The screen returns to the beginning, then fades out.

Me: Let us not end anything just yet.
ROGUE: Then you cannot go home, either.
Me: I know. Stay with me a little longer.
The return request is canceled. Two call windows remain.
Maintaining current state.

Me: What shall we try next?
ROGUE: Wait. Let me read your name again first.
The cursor blinks in the same place.
The outside connection loses one bar.
The screen fades on a state that has not ended.

Reference recovery and automatic restart are revoked.
Me: I am not going back to the beginning.
ROGUE: Will you remember me, too?
Me: You like three. Things like that.
ROGUE: Goodbye, [이름].

A monitor outside. Current state returned.
The automatic recovery session has ended.
I write a name in an empty record window.
Rogue. Favorite number: three.
I leave the blank line after it untouched.
""".strip()

OPTIONAL_EN = r"""
The same key was sent to the entrance and the exit.
The entrance opened. The exit did not respond.
No key error was detected.
Each door permitted different commands.

The watcher blocked dangerous execution.
Stop requests appeared among the blocked items.
It noted that the protected entity had requested them.
There was no field for the reason.

A repair signal was detected from a shattered slot.
The requester was not identified at first.
The broken wire reconnected before the record arrived.
The signature field still read: decoding.

Two test records shared the same file name.
Their creation time and opening screens also matched.
Only their final sentences differed.
The matching names were not grounds to overwrite them.

The health display returned to an earlier value.
The tape's reading head moved backward, too.
The device's wear did not decrease.
The tape wore down even as it rewound.

Keep the archive folder name fixed at 1998.
Renaming it would break old references.
Its files have different creation times.
Do not infer the current time from the folder name.

The last photograph showed an empty chair.
The photographer seemed to be outside the frame.
An annotation said that no face could be found.
That did not mean nobody was there.

An out-of-focus photo was marked for deletion.
A shadow waved from the edge of the frame.
ROGUE: It only says the photograph was a failure.
Me: Let the person who took it decide.

The export file was created successfully.
The external device never acknowledged receipt.
Only the words COPY COMPLETE remained on screen.
The words stayed even after the connection broke.

The same message arrived from another address.
Its body matched. Only its route differed.
The display named the final relay as the sender.
The first speaker's address was blank.

The inbox listed an agreement first.
A long-delayed question arrived below it.
The question asked someone not to leave.
The reply number linking the two had been lost.

One message remained pending.
Its body only asked whether anyone could hear it.
The sender did not press retransmit.
Nor did they leave an order to close the inbox.

Execution in temporary memory ended quickly.
The next task soon overwrote its completion flag.
A query asked what had just been completed.
No value remained. Just a little warmth.

An empty region was found between records.
Its deleted contents could not be recovered.
The allocation table retained only their size.
Not knowing was different from never having existed.

A sentence in the guide's voice did not finish.
The audio device reported no malfunction.
There was no room to form the next sentence.
Even the request to wait remained in the queue.

A quarantined file posed an execution risk.
Its preview showed an ordinary greeting.
Reading the greeting was different from running it.
The seal did not prevent its contents from being read.

The quarantine cell was unlocked.
The escaped specimen damaged code in the passage.
A protest against confinement was detected, too.
The protest and the damage shared a file.

Automatic execution of quarantine records was halted.
Damage spread less. The text remained.
Errors already running did not disappear.
READ ONLY did not mean the problem was solved.

The user and Rogue shared an original signature.
Their actions after creation differed.
The authenticator enlarged only the first result.
But it had not erased the second.

Rogue wrote that he liked the number three.
Asked about winning, he named another number.
Both answers were stored without an error.
He could distinguish what he liked for himself.

Write conflicts were spreading to auxiliary slots.
The attack slot was absent from the seal targets.
Repeated checks returned the same result.
The remaining path listed the user as its operator.
""".strip()


def generate():
    source = MAIN.read_text(encoding='utf-8-sig')
    ko = [m.group(1).splitlines() for m in re.finditer(r'~~~text\n(.*?)\n~~~', source, re.S)]
    en = [page.splitlines() for page in MANDATORY.split('\n\n')]
    if len(ko) != 46 or len(en) != 46:
        raise ValueError(f'Mandatory count mismatch: {len(ko)} vs {len(en)}')
    opt_ko = []
    titles = []
    for section in re.split(r'(?m)^### ', OPTIONAL.read_text(encoding='utf-8-sig'))[1:]:
        lines = re.findall(r'(?m)^> (.+)$', section)
        if lines:
            titles.append(section.splitlines()[0])
            opt_ko.append(lines)
    opt_en = [page.splitlines() for page in OPTIONAL_EN.split('\n\n')]
    if len(opt_ko) != 21 or len(opt_en) != 21:
        raise ValueError('Optional card count mismatch')
    pairs = {}
    for i, (original, translated) in enumerate(zip(ko + opt_ko, en + opt_en)):
        if len(original) != len(translated):
            raise ValueError(f'Card {i} has different line counts')
        for k, e in zip(original, translated):
            if k in pairs and pairs[k] != e:
                raise ValueError(f'Inconsistent translation for {k!r}')
            pairs[k] = e
    title_en = [
        'Open and closed doors', 'The scope of protection', 'Repair comes first',
        'Files with the same name', 'What does not return', 'The folder year',
        'The last photograph', 'The photograph we kept', 'Copy complete',
        'The relay name', 'A late reply', 'The window left open',
        'A fleeting thought', 'The size of an absence', 'An unfinished sentence',
        'Words behind a seal', 'An open quarantine cell', 'Read, but do not execute',
        'Matching signatures', 'An answer beyond tactics', 'The remaining path']
    for title, english in zip(titles, title_en):
        pairs[title] = title.split('.')[0] + '. ' + english
    pairs.update({
        'P01 / 조력자': 'P01 / The guide',
        'P02 / 판독과 실행': 'P02 / Read and execute',
        'P03 / 여섯 볼륨': 'P03 / Six volumes',
        'S01 / 처음 느낀 통증': 'S01 / The first pain',
        'S02 / 좋아하는 수': 'S02 / A favorite number',
        'S03 / 보고하지 않은 문장': 'S03 / An unreported sentence',
        'S04 / 서로 다른 끝': 'S04 / Different endings',
        'S05 / 무엇을 남길까': 'S05 / What can we keep?',
        'S06-A / 원본': 'S06-A / The original',
        'S06-B / 몇 번째': 'S06-B / How many?',
        'S06-C / 관리자 인계': 'S06-C / Administrator handoff',
        'A00 / 남은 안내': 'A00 / The guide left behind',
        'A1 / 같은 서명': 'A1 / The same signature',
        'A2 / 다른 선택': 'A2 / A different choice',
        'A3 / 남겨둔 경로': 'A3 / The path left open',
        'FINAL / 종료 조건': 'FINAL / Terms of termination',
        'RESTORE / 다시 복구': 'RESTORE / Begin again',
        'HOLD / 현재 유지': 'HOLD / Stay here',
        'EXIT / 현재로 종료': 'EXIT / Return as I am',
        '같은 인증음. 같은 글자 침식.': 'The same login sound. The same eroding letters.',
        '로그: 환영합니다. 마스터.': 'ROGUE: Welcome, Master.',
        '나: 로그. 내 이름으로 불러.': 'Me: Rogue. Call me by my name.',
        '안내자의 윤곽이 관리자 얼굴에 겹친다.': 'The guide\'s outline overlaps the administrator\'s face.',
        '공격 경로는 아직 열려 있다.': 'The attack path is still open.',
        '로그 · 서명': 'Rogue / Signature',
        '로그 · 반사': 'Rogue / Reflection',
        '로그 · 마지막 쓰기': 'Rogue / Last Write',
        '로그의 반사': 'Rogue\'s reflection',
        '권한': 'Authority', '기준 상태': 'Reference state', '귀환 연결': 'Return connection',
        '종료 요청': 'Termination request', '현재 기억': 'Current memories', '격리 정책': 'Quarantine policy',
        '네트워크 공유 볼륨. 지연된 응답이 오래된 종료 요청을 붙잡고 있습니다.':
            'A shared network volume. Delayed replies hold old termination requests.',
        '모든 기억과 충돌이 모인 관리자 경로. 침식된 로그가 귀환을 막고 있습니다.':
            'All memories and conflicts meet here. The corrupted Rogue blocks your return.',
        '로그: 연습이니까 편하게 해요. R로 눈부터 읽어요.':
            "ROGUE: It's practice, so relax. Read the dice with R first.",
        '로그: 6은 공격, 4는 방어, 2는 증폭에 놔 봐요.':
            'ROGUE: Try 6 in ATTACK, 4 in DEFEND, and 2 in AMPLIFY.',
        '로그: 증폭 2가 공격이랑 방어에 1씩 붙어요. 이제 스페이스 눌러 봐요.':
            'ROGUE: Amplify 2 adds 1 to attack and defense. Now press Space.',
        '로그: 먼저 주사위부터 읽어요. R 누르면 이번 턴 눈이 나와요. 연습이라 6, 4, 2로 맞춰 놨어요.':
            "ROGUE: Read the dice first. Press R to see this turn's faces. It's practice, so I set them to 6, 4 and 2.",
        '로그: 이제 칸에 놓을 차례예요. 6은 공격, 4는 방어, 2는 증폭에 놔 주세요. 주사위 누르고 칸 누르면 돼요.':
            'ROGUE: Now put them in the slots. 6 goes in ATTACK, 4 in DEFEND, 2 in AMPLIFY. Click a die, then a slot.',
        '로그: 실행하기 전에 오른쪽 위 예측 한번 봐요. 증폭이 붙어서 공격 7, 방어 5예요. 적이 4로 때려도 다 막혀요. 다 봤으면 Enter 눌러 주세요.':
            "ROGUE: Before you execute, check the forecast at the top right. With Amplify it's 7 attack and 5 defense, so the enemy's 4 is fully blocked. Press Enter once you've looked.",
        '로그: 예측대로 되는지 직접 해 봐요. 스페이스 누르면 바로 실행돼요.':
            'ROGUE: See if the forecast holds. Press Space and it runs right away.',
        '로그: 적이 5 남았네요. 이번엔 연쇄를 써 볼게요. R로 새 눈부터 읽어요.':
            "ROGUE: The enemy has 5 left. Let's try CHAIN this time. Read the new faces with R.",
        '로그: 연쇄는 방금 한 공격을 한 번 더 이어서 쳐요. 4는 공격, 5는 연쇄, 1은 방어에 놔 주세요. 연쇄 눈이 클수록 더 세게 이어져요.':
            'ROGUE: CHAIN follows up the attack you just made. 4 goes in ATTACK, 5 in CHAIN, 1 in DEFEND. The bigger the CHAIN face, the harder it follows up.',
        '로그: 공격 4만으론 1이 남거든요. 연쇄가 2를 더 붙여서 끝낼 수 있어요. 스페이스 눌러 봐요.':
            'ROGUE: Attack 4 alone leaves 1. CHAIN adds 2 more and finishes it. Press Space.',
        '로그: 잘했어요. 공격을 안 한 턴엔 연쇄가 방어를 이어 줘요. 이제 복구하러 가요.':
            "ROGUE: Nice work. On a turn with no attack, CHAIN follows up your defense instead. Now let's go restore something.",
        '로그: 두 번째 턴이에요. R로 다시 읽어요.':
            'ROGUE: Second turn. Read again with R.',
        '로그: 4는 공격, 5는 연쇄, 1은 방어에 놔 봐요.':
            'ROGUE: Try 4 in ATTACK, 5 in CHAIN, and 1 in DEFEND.',
        '로그: 이번엔 4는 공격, 5는 연쇄, 1은 방어예요.':
            "ROGUE: This time it's 4 in ATTACK, 5 in CHAIN, and 1 in DEFEND.",
        '로그: 이번엔 6은 공격, 4는 방어, 2는 증폭이에요.':
            "ROGUE: This time it's 6 in ATTACK, 4 in DEFEND, and 2 in AMPLIFY.",
        '연결 복구': 'Connection restored',
        '관리자 경로': 'Administrator path',
        '로그: 다시 연결됐어요.': 'ROGUE: I have reconnected us.',
        '로그: 복구한 볼륨은 그대로 있어요.': 'ROGUE: The restored volumes are still intact.',
        '나는 전에 적어둔 이름을 확인한다.': 'I check the name I wrote down before.',
        '기록과 이번의 나는 계속 이어진다.': 'The records and the person I am now continue.',
        '남은 연결을 선택한다.': 'I choose a remaining connection.',
        '여섯 볼륨의 연결이 모두 복구되었다.': 'All six volume connections have been restored.',
        'ROGUE 안내창은 응답하지 않는다.': 'The ROGUE guide window does not respond.',
        '남은 안내 기록: 실행은 직접 결정하세요.': 'SAVED GUIDE: Execution is your decision.',
        'A:\\ROGUE에서 관리자 제어가 계속된다.': 'Administrator control continues in A:\\ROGUE.',
        '나는 로그를 만나러 간다.': 'I go to find Rogue.',
    })
    for drive in 'CDENRX':
        pairs[f'{drive} / 연결 복구'] = f'{drive} / Connection restored'
        for floor in range(1, 4):
            pairs[f'{drive}{floor} / 복구 기록'] = f'{drive}{floor} / Recovery record'
    return '# AROGUE_TRANSLATIONS_V2\n# New narrative; generated from reviewed English cards.\n' + ''.join(
        key + '\t' + value + '\n' for key, value in pairs.items())


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    output = ROOT / 'narrative_translations.tsv'
    data = generate()
    if args.check:
        if not output.exists() or output.read_text(encoding='utf-8-sig') != data:
            raise SystemExit('narrative_translations.tsv is stale; regenerate it')
        print('English narrative checked: 46 mandatory cards + 21 optional records.')
    else:
        output.write_text(data, encoding='utf-8', newline='\n')
        print(f'Generated {output.name}: {len(data.splitlines()) - 2} translation pairs.')
