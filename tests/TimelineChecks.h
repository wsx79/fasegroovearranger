// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
void verifyTimelineEditing(){
    using namespace fga;
    auto fixture=[](){auto p=newProject();p.sections={{Section::main,4,"Main"},{Section::breakdown,4,"Break"}};p.normalise();for(auto& t:p.tracks)for(auto& c:t.clips)c.notes.clear();p.tracks[0].clips[0].notes={{2,8,60,100,0},{12,1,64,80,0}};p.tracks[4].clips[0].notes={{0,.25,36,90,0},{8,.25,36,60,0}};return p;};
    auto signature=[](const Project& p,int track){std::vector<std::array<double,5>> out;for(auto e:events(p,track))out.push_back({e.beat,(double)e.pitch,(double)e.velocity,(double)e.lane,(double)e.controller});return out;};
    auto p=fixture();const auto other=p.tracks[4].clips;check(p.splitClip(0,0,6)==1,"split clip inside a held note");
    check(p.tracks[0].freeTimeline&&p.tracks[0].clips.size()==3&&p.clipLength(0,0)==6&&p.clipStart(0,1)==6&&p.clipLength(0,1)==10,"split creates independently positioned clips without changing guide");
    check(p.tracks[0].clips[0].notes[0].length==4&&p.tracks[0].clips[1].notes[0].start==0&&p.tracks[0].clips[1].notes[0].length==4&&p.tracks[4].clips==other,"held note trimmed at split, other tracks untouched");
    const auto before=signature(p,0);const double inside=p.clipStart(0,1);int follower=-1;double followerAt=-1;
    for(int i=0;i<(int)p.tracks[0].clips.size();++i)if(p.clipStart(0,i)>=15.9){follower=i;followerAt=p.clipStart(0,i);}
    const bool resized=follower>=0&&p.resizeGuide(0,8)&&p.sections[0].bars==8&&p.sectionStart(1)==32&&p.clipStart(0,1)==inside&&std::abs(p.clipStart(0,follower)-(followerAt+16))<1e-6&&signature(p,0)==before;
    check(resized,"guide resize keeps music inside the section and moves later clips with the guide");
    {auto cut=fixture();cut.tracks[0].clips[0].notes={{1,1,60,100,0}};cut.tracks[0].clips[1].notes={{1,1,64,90,0}};cut.removeGuide(0);check(cut.sections.size()==1&&!cut.tracks[0].freeTimeline&&cut.tracks[0].clips.size()==1&&cut.tracks[0].clips[0].notes[0].pitch==64&&cut.clipStart(0,0)==0,"deleting a section removes its music and closes the gap");
        auto shifted=fixture();shifted.detachTrack(0);shifted.tracks[0].clips[0].notes={{1,1,60,100,0}};shifted.tracks[0].clips[1].notes={{2,1,67,80,0}};shifted.moveGuide(1,0);bool low=false,high=false;for(const auto& c:shifted.tracks[0].clips)for(const auto& n:c.notes){if(n.pitch==67)low=std::abs(c.position+n.start-2)<1e-6;if(n.pitch==60)high=std::abs(c.position+n.start-17)<1e-6;}check(shifted.sections[0].name=="Break"&&low&&high,"moving a section carries free clips with it");
        auto grown=fixture();grown.detachTrack(4);const double at=grown.clipStart(4,1);SongSection added{Section::main,4,"New"};const bool inserted=grown.insertSection(1,added);int found=-1;for(int i=0;i<(int)grown.tracks[4].clips.size();++i)if(std::abs(grown.clipStart(4,i)-(at+16))<1e-6)found=i;check(inserted&&found>=0&&grown.sectionStart(2)==32,"inserting a section shifts free clips and the following guide");}
    auto moving=fixture();const auto original=moving.tracks[0].clips[0].notes;const int moved=moving.moveClip(0,0,0,40,false);check(moved>=0&&moving.clipStart(0,moved)==40&&moving.tracks[0].clips[(size_t)moved].notes==original&&moving.clipAtBeat(0,2)<0,"move clip beyond song leaves a gap without swapping");check(moving.length()==56,"export includes moved clip past guide end");
    const auto copied=moving.moveClip(0,moved,1,4,true);check(copied>=0&&moving.tracks[1].freeTimeline&&moving.clipAtBeat(0,42)>=0,"copy between tracks preserves source and instrument routing");
    auto overwrite=fixture();Clip insert;insert.span=4;insert.notes={{0,1,72,77,0}};check(overwrite.placeClip(0,insert,6)>=0,"paste into middle of existing clip");check(overwrite.tracks[0].clips.size()==4&&overwrite.clipAtBeat(0,5)>=0&&overwrite.clipAtBeat(0,6)>=0&&overwrite.clipAtBeat(0,11)>=0,"overwrite paste preserves both outside fragments");
    bool overlap=false;double end=0;for(auto& c:overwrite.tracks[0].clips){overlap=overlap||c.position<end;end=c.position+c.span;}check(!overlap,"overwrite never doubles overlapping MIDI");
    const int pasted=overwrite.clipAtBeat(0,6);check(overwrite.resizeClipLoop(0,pasted,8),"loop extension after split and paste");check(overwrite.tracks[0].clips[(size_t)overwrite.clipAtBeat(0,6)].notes.size()==2,"independent clip repeats MIDI on extension");
    while(!overwrite.tracks[0].clips.empty())check(overwrite.deleteClip(0,0),"delete last clip");overwrite.normalise();check(overwrite.tracks[0].clips.empty()&&events(overwrite,0).empty(),"empty edited track remains empty after normalise");
    auto pedal=fixture();pedal.sections[0].type=Section::buildup;pedal.tracks[0].engine.pedalCC=true;const double beforeLen=pedal.clipLength(0,0);const bool extended=pedal.resizeGuide(0,8)&&pedal.clipLength(0,0)==32&&pedal.clipLength(0,0)>beforeLen;check(extended,"guide resize extends the aligned clip with the section");const auto buildup=signature(pedal,0);pedal.sections[0].type=Section::outro;check(signature(pedal,0)!=buildup,"section role still drives expression after the guide resize");
    auto manual=fixture();manual.tracks[0].freeTimeline=true;manual.tracks[0].clips.clear();manual.tracks[0].engine.pedalCC=true;Clip drawn;drawn.span=4;drawn.notes={{0,2,60,100,0}};manual.placeClip(0,drawn,0);auto manualBefore=signature(manual,0);manual.sections[0].type=Section::buildup;manual.normalise();check(signature(manual,0)==manualBefore,"manual clips capture controller role independently of guide edits");
    auto generated=fixture();generated.splitClip(0,0,4);generated.splitClip(0,1,8);generated.splitClip(0,2,12);generate(generated,Scope::clip,0,3,42);check(!generated.tracks[0].clips[3].notes.empty()&&generated.clipStart(0,3)==12&&generated.clipLength(0,3)==4,"regenerate split clip with index beyond guide count keeps its position and length");
    auto invalid=fixture();check(invalid.splitClip(0,0,0)<0&&invalid.moveClip(0,0,99,4,false)<0&&!invalid.deleteClip(-1,0),"invalid edits rejected");
    {
        check(recognizeChordPitches({60,64,67},-1,0)=="C","C major triad");
        check(recognizeChordPitches({64,67,72},-1,0)=="C","first inversion stays C");
        check(recognizeChordPitches({48,60,64,67,76},-1,0)=="C","octave doubles stay one chord");
        check(recognizeChordPitches({60,63,67},-1,0)=="Cm","C minor");
        check(recognizeChordPitches({60,64,67,71},-1,0)=="Cmaj7","major seventh");
        check(recognizeChordPitches({65,69,72,74},-1,0)=="Dm7","minor seventh inversion");
        check(recognizeChordPitches({67,71,74,77},-1,0)=="G7","dominant seventh");
        check(recognizeChordPitches({60,64},-1,0).empty()&&recognizeChordPitches({},-1,0).empty(),"pairs and silence are not chords");
        check(recognizeChordPitches({60,64,66},-1,0).empty(),"unrelated pitches are not labelled");
        const std::string planned="Am";
        std::vector<Note> held{{0,4,60,100,0},{0,4,64,100,0},{0,4,67,100,0}};
        check(chordHeardInClip(held,0,4,-1,0,planned)=="C","overlapping triad replaces the planned name");
        std::vector<Note> line{{0,1,60,100,0},{1,1,64,100,0},{2,1,67,100,0}};
        check(chordHeardInClip(line,0,4,-1,0,planned)==planned,"a melody is not stacked into a chord");
        std::vector<Note> both{{0,2,60,100,0},{0,2,64,100,0},{0,2,67,100,0},{2,2,62,100,0},{2,2,65,100,0},{2,2,69,100,0}};
        check(chordHeardInClip(both,0,4,-1,0,planned)==planned,"two different chords in one cell keep the section harmony");
        check(chordHeardInClip({},0,4,-1,0,planned)==planned,"silence keeps the section harmony");
    }
    {
        auto song=newProject(1);song.sections={{Section::main,4,"Main"},{Section::drop,4,"Drop"}};song.normalise();
        song.sections[1].harmony.enabled=true;song.sections[1].harmony.key=0;song.sections[1].harmony.scale=0;song.sections[1].harmony.chordCount=2;song.sections[1].harmony.chordSteps=8;song.sections[1].harmony.degrees={0,4,0,0,0,0,0,0};
        double a0=0,b0=0,a1=0,b1=0,a2=0,b2=0;std::string n0,n1,n2;chordCell(song,0,a0,b0,n0);chordCell(song,16,a1,b1,n1);chordCell(song,18,a2,b2,n2);
        check(n0==chordNameAt(song,0)&&n1==chordNameAt(song,16)&&n1!=n0,"ruler cells follow each section's harmony instead of the selected progression");
        check(std::abs(a1-16)<1e-9&&std::abs(b1-a1-2)<1e-9&&std::abs(a2-18)<1e-9&&n2==chordNameAt(song,18)&&n2!=n1,"section chord steps clip at the boundary and advance inside the section");
    }
    {
        auto group=newProject();group.sections={{Section::main,8,"Main"}};group.normalise();
        group.tracks[4].clips[0].notes={{0,.25,60,100,0}};group.tracks[4].clips[0].locked=true;group.tracks[4].clips[0].name="Kick phrase";
        group.tracks[5].clips[0].notes={{1,.25,62,90,0}};group.tracks[5].clips[0].name="  Backbeat  ";group.normalise();
        check(group.tracks[4].clips[0].name=="Kick phrase"&&group.tracks[5].clips[0].name=="Backbeat","clip names trim and survive normalisation");
        const auto saved=group;std::vector<ClipPlacement> placed;double applied=0;
        check(group.translateClipGroup({{4,0},{5,0}},8,0,false,applied,placed)&&applied==8,"group move keeps relative timing");
        const int kick=group.clipAtBeat(4,8.1),snare=group.clipAtBeat(5,8.1);check(kick>=0&&snare>=0&&group.clipAtBeat(4,0.1)<0&&group.clipAtBeat(5,1.1)<0,"group move leaves the original beats empty");
        check(group.tracks[4].clips[(size_t)kick].locked&&group.tracks[4].clips[(size_t)kick].name=="Kick phrase"&&group.tracks[4].clips[(size_t)kick].notes[0].pitch==fgd::defaultNote(0),"moved clip keeps its lock, name and drum pitch");
        check(std::abs(group.clipStart(5,snare)-8)<1e-6&&group.tracks[5].clips[(size_t)snare].notes[0].start==1,"the later clip keeps its offset inside the group");
        auto vertical=saved;check(vertical.translateClipGroup({{4,0},{5,0}},0,1,false,applied,placed),"group moves down one row together");
        const int movedKick=vertical.clipAtBeat(5,0.1),movedSnare=vertical.clipAtBeat(6,0.1);
        check(movedKick>=0&&movedSnare>=0&&vertical.clipAtBeat(4,0.1)<0&&vertical.tracks[5].clips[(size_t)movedKick].notes[0].pitch==fgd::defaultNote(1)&&vertical.tracks[6].clips[(size_t)movedSnare].notes[0].pitch==fgd::defaultNote(2)&&vertical.tracks[5].clips[(size_t)movedKick].locked,"vertical move preserves rows and remaps each drum");
        auto rejected=saved;check(!rejected.translateClipGroup({{4,0}},0,-1,false,applied,placed)&&rejected.tracks[4].clips[0]==saved.tracks[4].clips[0],"a group that would leave the tracks is rejected whole");
        auto copied=saved;check(copied.translateClipGroup({{4,0}},4,0,true,applied,placed)&&copied.clipAtBeat(4,0.1)>=0&&copied.clipAtBeat(4,4.1)>=0&&copied.tracks[4].clips[(size_t)copied.clipAtBeat(4,4.1)].name=="Kick phrase","copy keeps the source and the name");
        std::vector<ClipGroupPiece> pieces;double origin=0,end=0;check(saved.collectClipGroup({{4,0},{5,0}},4,pieces,origin,end)&&pieces.size()==2&&pieces[0].rowDelta==0&&pieces[1].rowDelta==1,"collected group keeps row offsets");
        auto pasted=saved;std::vector<ClipPlacement> again;check(pasted.placeClipGroup(4,32,pieces,again)&&pasted.clipAtBeat(4,32.1)>=0&&pasted.clipAtBeat(5,32.1)>=0&&pasted.clipAtBeat(4,0.1)>=0,"paste keeps track offsets and does not remove the source");
        auto removed=saved;check(removed.removeClipGroup({{5,0}})&&removed.clipAtBeat(5,1)<0&&removed.tracks[4].clips[0].locked,"removing one clip leaves the other locked clip");
    }
}
