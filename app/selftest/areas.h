// venv-vrequency self-test — every group of checks, in the order they run. The order matters: each
// group starts from the state the ones before it left in the app (the loop the looper recorded is the
// one the session saves, the control socket clears...).
#pragma once

class MainWindow;
class QString;
namespace td { class Instrument; }

namespace selftest {
struct Context;
struct Link;
struct TourContext;

void testAudio(Context &ctx);            // audio_layout.cpp
void testLayout(Context &ctx);
void testKeys(Context &ctx);             // keys.cpp
void testSounds(Context &ctx);           // sounds.cpp
void testBacking(Context &ctx);          // backing.cpp
void testLooper(Context &ctx);           // looper.cpp
void testTouch(Context &ctx);            // touch.cpp
void testSaving(Context &ctx);           // saving.cpp
void testBeat(Context &ctx);             // beat.cpp
void testKeptBeats(Context &ctx);        // kept_beats.cpp
void testListeningBack(Context &ctx);    // listening.cpp
void testControlSocket(Context &ctx);    // socket.cpp, which runs:
void socketNotes(Context &ctx, Link &link);                // socket_notes.cpp
void socketEveryCommand(Context &ctx, Link &link);         // socket_commands.cpp
void socketChat(Context &ctx, Link &link);                 // socket_chat.cpp
void socketHostileInput(Context &ctx, Link &link);         // socket_hostile.cpp
void testGuidedHelp(Context &ctx);       // guided_help.cpp, which runs:
void guideZoneListing(Context &ctx, Link &link);           // guided_zones.cpp
void guideHighlights(Context &ctx, Link &link);
void guideHostileArguments(Context &ctx, Link &link);
void guideDock(Context &ctx, Link &link);                  // guided_dock.cpp
void guideTicker(Context &ctx);
void guideDockAvoidsHighlight(Context &ctx, Link &link);   // guided_focus.cpp
void guideDockAvoidsEveryZone(Context &ctx);
void guideHelpPage(Context &ctx, Link &link);
void testBugFixes(Context &ctx);         // bugfixes.cpp
// runTourTests (tour.cpp) comes here, and runs:
void tourCards(TourContext &ctx);                          // tour_cards.cpp
void tourWaysIn(TourContext &ctx);                         // tour_open.cpp
void tourPictures(TourContext &ctx);                       // tour_pictures.cpp
void tourSelection(TourContext &ctx);                      // tour_select.cpp
void tourReachesNothing(TourContext &ctx);                 // tour_inert.cpp
void tourAskRook(TourContext &ctx);                        // tour_ask.cpp
void tourConcepts(TourContext &ctx);                       // tour_concepts.cpp
void tourGuidedWalk(TourContext &ctx);
void tourFirstRunOffer(TourContext &ctx);                  // tour_first_run.cpp
void tourRemembersLastPage(TourContext &ctx);
void tourDamagedCards(TourContext &ctx);                   // tour_cards.cpp
void tourCardsFit(TourContext &ctx);
void tourPaintTime(TourContext &ctx);                      // tour_limits.cpp
void tourHostileArguments(TourContext &ctx);
void testPanicAndHealth(Context &ctx);   // health.cpp
} // namespace selftest
