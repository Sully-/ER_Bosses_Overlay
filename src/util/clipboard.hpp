#pragma once
#include <windows.h>
#include <string>

namespace er::util {

    void CopyToClipboard(const std::string& text) {
        // Ouvrir le presse-papiers
        if (OpenClipboard(nullptr)) {
            // Vider le presse-papiers actuel
            EmptyClipboard();

            // Allouer de la m�moire pour le texte � copier (global allocation)
            HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
            if (hGlob) {
                // Obtenir un pointeur vers la m�moire allou�e
                char* buffer = (char*)GlobalLock(hGlob);
                memcpy(buffer, text.c_str(), text.size() + 1);
                GlobalUnlock(hGlob);

                // D�finir le texte dans le presse-papiers
                SetClipboardData(CF_TEXT, hGlob);
            }

            // Fermer le presse-papiers
            CloseClipboard();
        }
    }
}