// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "GuidePage.h"
#include "FamilyTheme.h"
#include "Branding.h"
#include <BinaryData.h>
#include <functional>
#include <cmath>
namespace fga {
namespace {
juce::String slug(juce::String s){s=s.toLowerCase().replaceCharacter(0x00e0,'a').replaceCharacter(0x00e8,'e').replaceCharacter(0x00e9,'e').replaceCharacter(0x00ec,'i').replaceCharacter(0x00f2,'o').replaceCharacter(0x00f9,'u').replaceCharacter(' ','-');return s.retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789-");}
juce::Image guideImage(const juce::String& name){
    if(name=="logo.svg"){juce::Image image(juce::Image::ARGB,560,160,true);juce::Graphics g(image);g.fillAll(Palette::background);Branding::drawFullLogo(g,{0,28,560,104});return image;}
    if(name=="options.png")return juce::ImageCache::getFromMemory(BinaryData::options_png,BinaryData::options_pngSize);
    if(name=="screenshot.png"||name.endsWith("guide_arrangement.png"))return juce::ImageCache::getFromMemory(BinaryData::guide_arrangement_png,BinaryData::guide_arrangement_pngSize);
    if(name.endsWith("guide_piano.png"))return juce::ImageCache::getFromMemory(BinaryData::guide_piano_png,BinaryData::guide_piano_pngSize);
    if(name.endsWith("guide_guitar.png"))return juce::ImageCache::getFromMemory(BinaryData::guide_guitar_png,BinaryData::guide_guitar_pngSize);
    if(name.endsWith("guide_mixer.png"))return juce::ImageCache::getFromMemory(BinaryData::guide_mixer_png,BinaryData::guide_mixer_pngSize);
    return {};
}
struct InlineText{juce::AttributedString styled;juce::String plain,target;};
InlineText inlineText(const juce::String& text,float size,bool heading=false,juce::Colour colour=Palette::text){
    InlineText out;out.styled.setLineSpacing(3);bool bold=heading,code=false,italic=false;
    auto append=[&](juce::String s,bool link=false){auto font=FamilyTheme::font(size,bold);if(italic)font=font.italicised();if(link)font.setUnderline(true);out.styled.append(s,font,link?Palette::accent2:code?Palette::accent:colour);out.plain+=s;};
    for(int i=0;i<text.length();){
        if(text.substring(i,i+2)=="**"){bold=!bold;i+=2;continue;}
        if(text[i]=='*'&&!code){italic=!italic;++i;continue;}
        if(text[i]=='`'){code=!code;++i;continue;}
        if(text[i]=='['){const int close=text.indexOf(i,"]("),end=close>=0?text.indexOf(close+2,")"):-1;if(close>=0&&end>=0){append(text.substring(i+1,close),true);if(out.target.isEmpty())out.target=text.substring(close+2,end);i=end+1;continue;}}
        int end=i+1;while(end<text.length()&&text[end]!='*'&&text[end]!='`'&&text[end]!='[')++end;
        // Keep the family manuals’ inline emphasis and links in the same source Markdown.
        append(text.substring(i,end));i=end;
    }return out;
}
class PicturePage:public juce::Component{
public:
    explicit PicturePage(juce::Image image){setLookAndFeel(&theme);picture.setImage(image);picture.setSize(image.getWidth(),image.getHeight());port.setViewedComponent(&picture,false);addAndMakeVisible(port);setSize(1060,760);}
    ~PicturePage()override{setLookAndFeel(nullptr);}
    void resized()override{port.setBounds(getLocalBounds());}
private:FamilyLookAndFeel theme;juce::ImageComponent picture;juce::Viewport port;
};
class GuideBook:public juce::Component{
public:
    enum class Kind{title,chapter,heading,paragraph,list,quote,table,image,rule,code};
    struct Cell{juce::String text;juce::Rectangle<float> area;std::unique_ptr<juce::TextLayout> layout;};
    struct Block{Kind kind=Kind::paragraph;juce::String text,plain,target,anchor;juce::Rectangle<float> area,imageArea;juce::Image image;float preferredWidth=0;int depth=0;bool caption=true;std::unique_ptr<juce::TextLayout> layout;std::vector<std::vector<Cell>> rows;};
    std::vector<Block> blocks;std::vector<std::pair<juce::String,int>> chapters;
    std::function<void(juce::String)> navigate;
    float fontSize=15;juce::String query;
    void setMarkdown(const juce::String& md){
        blocks.clear();chapters.clear();auto lines=juce::StringArray::fromLines(md);
        auto cells=[](juce::String line){juce::StringArray result;line=line.trimCharactersAtStart("|").trimCharactersAtEnd("|");for(auto token:juce::StringArray::fromTokens(line,"|",""))result.add(token.trim());return result;};
        auto special=[](const juce::String& s){return s.isEmpty()||s.startsWith("#")||s.startsWith("|")||s.startsWith(">")||s.startsWith("![")||s.startsWith("<p ")||s.startsWith("- ")||s=="---"||s.startsWith("```")||(s.length()>2&&juce::CharacterFunctions::isDigit(s[0])&&s.contains(". "));};
        for(int i=0;i<lines.size();){auto raw=lines[i];auto line=raw.trim();if(line.isEmpty()){++i;continue;}Block b;
            if(line.startsWith("```")){b.kind=Kind::code;++i;while(i<lines.size()&&!lines[i].trim().startsWith("```")){b.text+=lines[i++]+"\n";}if(i<lines.size())++i;}
            else if(line=="---"){b.kind=Kind::rule;++i;}
            else if(line.startsWith("<p ")&&line.contains("<img ")){
                auto attr=[&](const char* key){auto mark=juce::String(key)+"=\"";const int start=line.indexOf(mark);if(start<0)return juce::String();const int from=start+mark.length();return line.substring(from,line.indexOf(from,"\""));};
                b.kind=Kind::image;b.caption=false;b.text=attr("alt");b.image=guideImage(attr("src"));b.preferredWidth=attr("width").getFloatValue();++i;
            }
            else if(line.startsWith("![")){b.kind=Kind::image;const int mid=line.indexOf("]("),end=line.lastIndexOf(")");b.text=line.substring(2,mid);b.image=guideImage(line.substring(mid+2,end));++i;}
            else if(line.startsWith("|")){b.kind=Kind::table;while(i<lines.size()&&lines[i].trim().startsWith("|")){auto row=cells(lines[i++].trim());bool separator=true;for(auto cell:row)if(cell.removeCharacters("-: ").isNotEmpty())separator=false;if(separator)continue;std::vector<Cell> data;for(auto cell:row){Cell c;c.text=cell;data.push_back(std::move(c));}b.rows.push_back(std::move(data));}}
            else if(line.startsWith("# ")){b.kind=Kind::title;b.text=line.substring(2);++i;}
            else if(line.startsWith("## ")){b.kind=Kind::chapter;b.text=line.substring(3);b.anchor=slug(b.text);chapters.push_back({b.text,(int)blocks.size()});++i;}
            else if(line.startsWith("### ")||line.startsWith("#### ")){const int level=line.startsWith("#### ")?5:4;b.kind=Kind::heading;b.text=line.substring(level);b.anchor=slug(b.text);++i;}
            else if(line.startsWith("> ")){b.kind=Kind::quote;while(i<lines.size()&&lines[i].trim().startsWith("> "))b.text+=lines[i++].trim().substring(2)+" ";}
            else if(line.startsWith("- ")||(line.length()>2&&juce::CharacterFunctions::isDigit(line[0])&&line.contains(". "))){b.kind=Kind::list;b.depth=std::clamp((raw.length()-raw.trimStart().length())/2,0,4);b.text=line.startsWith("- ")?juce::String::fromUTF8("• ")+line.substring(2):line;++i;}
            else{b.text=line;++i;while(i<lines.size()&&!special(lines[i].trim()))b.text+=" "+lines[i++].trim();}
            blocks.push_back(std::move(b));
        }laidOutWidth=-1;relayout();
    }
    void relayout(){
        if(getWidth()<120)return;laidOutWidth=getWidth();const float width=(float)getWidth()-64;float y=26;
        for(auto& b:blocks){
            const bool heading=b.kind==Kind::title||b.kind==Kind::chapter||b.kind==Kind::heading;
            if(b.kind==Kind::chapter)y+=24;else if(b.kind==Kind::heading)y+=12;
            b.area={32,y,width,0};b.plain.clear();b.target.clear();
            if(b.kind==Kind::rule){b.area.setHeight(16);}
            else if(b.kind==Kind::image){const float scale=b.image.isValid()?std::min({width/b.image.getWidth(),(b.preferredWidth>0?b.preferredWidth:width)/b.image.getWidth(),420.f/b.image.getHeight()}):1;b.imageArea={32+(width-b.image.getWidth()*scale)/2,y,b.image.getWidth()*scale,b.image.getHeight()*scale};b.area.setHeight(b.imageArea.getHeight()+(b.caption?36:12));b.plain=b.text;}
            else if(b.kind==Kind::table){float rowY=y;for(size_t r=0;r<b.rows.size();++r){auto& row=b.rows[r];float height=0,x=32;for(size_t c=0;c<row.size();++c){float cw=width/(float)std::max((size_t)1,row.size());if(row.size()==2)cw=width*(c==0?.31f:.69f);auto text=inlineText(row[c].text,fontSize-1,r==0,r==0?Palette::accent:Palette::text);row[c].layout=std::make_unique<juce::TextLayout>();row[c].layout->createLayout(text.styled,cw-24);row[c].area={x,rowY,cw,0};height=std::max(height,row[c].layout->getHeight()+24);x+=cw;b.plain+=text.plain+" ";}for(auto& c:row)c.area.setHeight(height);rowY+=height;}b.area.setHeight(rowY-y);}
            else{const float size=b.kind==Kind::title?29:b.kind==Kind::chapter?23:b.kind==Kind::heading?18:fontSize;auto text=inlineText(b.text,size,heading,b.kind==Kind::chapter?Palette::accent:Palette::text);b.plain=text.plain;b.target=text.target;b.layout=std::make_unique<juce::TextLayout>();const float inset=b.kind==Kind::quote?32:b.kind==Kind::list?12.f+b.depth*16.f:0.f;b.layout->createLayout(text.styled,width-inset);b.area.setHeight(b.layout->getHeight()+(b.kind==Kind::quote?26:0));}
            y+=b.area.getHeight()+(b.kind==Kind::list?7:14);
        }setSize(getWidth(),(int)std::ceil(y+26));repaint();
    }
    void resized()override{if(getWidth()!=laidOutWidth)relayout();}
    void paint(juce::Graphics& g)override{
        g.fillAll(Palette::field);const auto visible=g.getClipBounds().toFloat();
        for(const auto& b:blocks){if(!visible.intersects(b.area))continue;
            if(query.isNotEmpty()&&b.plain.containsIgnoreCase(query)){g.setColour(Palette::accent.withAlpha(.09f));g.fillRoundedRectangle(b.area.expanded(7,4),5);}
            if(b.kind==Kind::rule){g.setColour(Palette::panelEdge);g.drawHorizontalLine((int)b.area.getCentreY(),b.area.getX(),b.area.getRight());}
            else if(b.kind==Kind::image){if(b.image.isValid())g.drawImage(b.image,b.imageArea);if(b.caption){g.setColour(Palette::dim);g.setFont(FamilyTheme::font(12));g.drawText(b.text,b.area.withTop(b.imageArea.getBottom()+8),juce::Justification::centredTop);}}
            else if(b.kind==Kind::table){for(size_t r=0;r<b.rows.size();++r)for(const auto& c:b.rows[r]){g.setColour(r==0?Palette::control:r%2?Palette::panel:Palette::background);g.fillRect(c.area);g.setColour(Palette::panelEdge);g.drawRect(c.area,1);c.layout->draw(g,c.area.reduced(12,12));}}
            else if(b.layout){auto area=b.area;if(b.kind==Kind::quote){g.setColour(Palette::accent.withAlpha(.07f));g.fillRoundedRectangle(area.expanded(8,0),5);g.setColour(Palette::accent);g.fillRect(area.getX()-8,area.getY(),3.f,area.getHeight());area=area.reduced(16,13);}else if(b.kind==Kind::list)area.removeFromLeft(12.f+b.depth*16.f);b.layout->draw(g,area);}
        }
    }
    void mouseMove(const juce::MouseEvent& e)override{bool link=false;for(const auto& b:blocks)if(b.area.contains(e.position)&&(b.target.isNotEmpty()||b.image.isValid()))link=true;setMouseCursor(link?juce::MouseCursor::PointingHandCursor:juce::MouseCursor::NormalCursor);}
    void mouseDown(const juce::MouseEvent& e)override{for(const auto& b:blocks)if(b.area.contains(e.position)){if(b.image.isValid()){juce::DialogWindow::LaunchOptions o;o.dialogTitle=b.text;o.dialogBackgroundColour=Palette::field;o.useNativeTitleBar=true;o.resizable=true;o.content.setOwned(new PicturePage(b.image));o.launchAsync();}else if(b.target.isNotEmpty()&&navigate)navigate(b.target);return;}}
    int positionFor(const juce::String& anchor)const{for(const auto& b:blocks)if(b.anchor==anchor)return (int)b.area.getY()-20;return 0;}
    std::vector<int> find(const juce::String& s){query=s;std::vector<int> hits;if(s.isNotEmpty())for(const auto& b:blocks)if(b.plain.containsIgnoreCase(s))hits.push_back((int)b.area.getY()-20);repaint();return hits;}
private:int laidOutWidth=-1;
};
class GuidePage:public juce::Component,private juce::ListBoxModel{
public:
    explicit GuidePage(bool english):isEnglish(english),manual(true){initialise();loadManual();}
    GuidePage(juce::String title,juce::String markdown):documentTitle(std::move(title)){initialise();book.setMarkdown(markdown);index.updateContent();}
    ~GuidePage()override{index.setModel(nullptr);setLookAndFeel(nullptr);}
    void initialise(){
        setLookAndFeel(&theme);setWantsKeyboardFocus(true);port.setViewedComponent(&book,false);port.setScrollBarsShown(true,false);index.setModel(this);index.setRowHeight(37);
        for(juce::Component* c:std::initializer_list<juce::Component*>{&port,&index,&search,&previous,&next,&smaller,&larger,&language,&close,&result})addAndMakeVisible(c);
        search.setTextToShowWhenEmpty("Cerca nella guida / Search",Palette::dim);search.onTextChange=[this]{hits=book.find(search.getText());currentHit=-1;findNext(1);};
        previous.onClick=[this]{findNext(-1);};next.onClick=[this]{findNext(1);};smaller.onClick=[this]{zoom(-1);};larger.onClick=[this]{zoom(1);};
        language.addItem("Italiano",1);language.addItem("English",2);language.setSelectedId(isEnglish?2:1,juce::dontSendNotification);language.onChange=[this]{isEnglish=language.getSelectedId()==2;loadManual();};language.setVisible(manual);
        close.onClick=[this]{if(auto* w=findParentComponentOfClass<juce::DialogWindow>())w->closeButtonPressed();};
        book.navigate=[this](juce::String target){if(target.startsWithChar('#'))port.setViewPosition(0,book.positionFor(target.substring(1)));else if(manual&&(target=="GUIDA.md"||target=="GUIDE.md")){isEnglish=target=="GUIDE.md";language.setSelectedId(isEnglish?2:1,juce::dontSendNotification);loadManual();}else if(target.startsWith("https://"))juce::URL(target).launchInDefaultBrowser();};
        setSize(1100,800);
    }
    void loadManual(){book.setMarkdown(juce::String::fromUTF8(isEnglish?BinaryData::GUIDE_md:BinaryData::GUIDA_md,isEnglish?BinaryData::GUIDE_mdSize:BinaryData::GUIDA_mdSize));documentTitle=isEnglish?"USER GUIDE":"GUIDA DI UTILIZZO";close.setButtonText(isEnglish?"CLOSE":"CHIUDI");search.clear();index.updateContent();index.selectRow(0);port.setViewPosition(0,0);repaint();}
    void paint(juce::Graphics& g)override{g.fillAll(Palette::background);Branding::drawFullLogo(g,{20,12,294,54});g.setColour(Palette::dim);g.setFont(FamilyTheme::font(12,true));g.drawText(documentTitle,336,24,getWidth()-580,28,juce::Justification::centredLeft);g.setColour(Palette::panelEdge);g.fillRect(16,77,getWidth()-32,1);g.fillRect(238,128,1,getHeight()-164);g.setColour(Palette::dim);g.setFont(FamilyTheme::font(10,true));g.drawText(isEnglish?"CONTENTS":"INDICE",24,94,190,22,juce::Justification::centredLeft);g.setFont(FamilyTheme::font(11));g.drawText("FS PITCH  /  FASE GROOVE",20,getHeight()-27,260,20,juce::Justification::centredLeft);g.drawText(isEnglish?"Click a screenshot to enlarge it":"Clicca una schermata per ingrandirla",300,getHeight()-27,getWidth()-320,20,juce::Justification::centredRight);}
    void resized()override{language.setBounds(getWidth()-240,25,120,29);close.setBounds(getWidth()-110,25,90,29);search.setBounds(258,91,getWidth()-580,29);previous.setBounds(getWidth()-310,91,36,29);next.setBounds(getWidth()-268,91,36,29);result.setBounds(getWidth()-225,91,70,29);smaller.setBounds(getWidth()-150,91,58,29);larger.setBounds(getWidth()-86,91,58,29);index.setBounds(16,128,210,getHeight()-164);port.setBounds(250,134,getWidth()-266,getHeight()-173);book.setSize(port.getWidth()-16,book.getHeight());}
    bool keyPressed(const juce::KeyPress& key)override{if(key.getModifiers().isCommandDown()&&key.getKeyCode()=='F'){search.grabKeyboardFocus();return true;}return false;}
    void zoom(int delta){book.fontSize=std::clamp(book.fontSize+(float)delta,13.f,21.f);book.relayout();hits=book.find(search.getText());currentHit=-1;findNext(1);}
    void findNext(int direction){if(hits.empty()){result.setText(search.getText().isEmpty()?"":isEnglish?"No results":"Nessuno",juce::dontSendNotification);return;}currentHit=(currentHit+direction+(int)hits.size())%(int)hits.size();port.setViewPosition(0,hits[(size_t)currentHit]);result.setText(juce::String(currentHit+1)+" / "+juce::String((int)hits.size()),juce::dontSendNotification);}
    bool evidence(const juce::File& folder){
        bool content=book.chapters.size()==9&&std::count_if(book.blocks.begin(),book.blocks.end(),[](const auto& b){return b.kind==GuideBook::Kind::image&&b.image.isValid();})==6&&std::count_if(book.blocks.begin(),book.blocks.end(),[](const auto& b){return b.kind==GuideBook::Kind::table;})>=10;
        for(const auto& b:book.blocks)if(b.kind==GuideBook::Kind::table){content=content&&!b.rows.empty()&&b.rows.front().size()>=2;for(const auto& row:b.rows)content=content&&row.size()==b.rows.front().size();}
        auto shot=[&](const char* name){auto file=folder.getChildFile(juce::String(name)+(isEnglish?"-en":"-it")+".png");if(auto out=file.createOutputStream()){juce::PNGImageFormat png;return png.writeImageToStream(createComponentSnapshot(getLocalBounds()),*out);}return false;};
        port.setViewPosition(0,0);bool ok=shot("guide-welcome");for(const auto& block:book.blocks)if(block.target.startsWithChar('#'))ok=book.positionFor(block.target.substring(1))>0&&ok;index.selectRow(2);ok=port.getViewArea().getY()>0&&shot("guide-chapter")&&ok;search.setText(isEnglish?"NEW ARRANGEMENT":juce::String::fromUTF8("NUOVO ARRANGIAMENTO"));search.onTextChange();ok=!hits.empty()&&shot("guide-search")&&ok;zoom(1);ok=book.fontSize==16&&ok;search.clear();return content&&ok;
    }
private:
    int getNumRows()override{return (int)book.chapters.size();}
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selected)override{if(row<0||row>=getNumRows())return;if(selected){g.setColour(Palette::accent.withAlpha(.12f));g.fillRoundedRectangle(juce::Rectangle<float>(0,1,(float)width,(float)height-2),4);g.setColour(Palette::accent);g.fillRect(0,8,3,height-16);}g.setColour(selected?Palette::text:Palette::dim);g.setFont(FamilyTheme::font(12,selected));g.drawFittedText(book.chapters[(size_t)row].first,12,0,width-20,height,juce::Justification::centredLeft,2,1.f);}
    void selectedRowsChanged(int row)override{if(row>=0&&row<getNumRows())port.setViewPosition(0,(int)book.blocks[(size_t)book.chapters[(size_t)row].second].area.getY()-20);}
    juce::String getTooltipForRow(int row)override{return row>=0&&row<getNumRows()?book.chapters[(size_t)row].first:juce::String();}
    bool isEnglish=false,manual=false;juce::String documentTitle;
    FamilyLookAndFeel theme;GuideBook book;juce::Viewport port;juce::ListBox index;
    juce::TextEditor search;juce::TextButton previous{"<"},next{">"},smaller{"A-"},larger{"A+"},close{"CHIUDI"};juce::ComboBox language;juce::Label result;
    std::vector<int> hits;int currentHit=-1;
};
}
std::unique_ptr<juce::Component> createGuidePage(bool english){return std::make_unique<GuidePage>(english);}
std::unique_ptr<juce::Component> createReferencePage(const juce::String& title,const juce::String& markdown){return std::make_unique<GuidePage>(title,markdown);}
bool verifyGuidePages(const juce::File& folder){GuidePage italian(false),english(true);const bool ok=italian.evidence(folder)&&english.evidence(folder);folder.getChildFile("guide.txt").replaceWithText(ok?"PASS Piano-family manuals: nine chapters, six images including cover/settings, tables, navigation, search and zoom":"FAIL guide rendering");return ok;}
}
