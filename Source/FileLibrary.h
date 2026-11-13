#pragma once
#include <juce_core/juce_core.h>

namespace dr
{
// File usati di recente e preferiti, per i profili NAM (ampli e pedale) e per le IR.
// Salvati in <dati utente>/DaniRig/library.xml, condivisi tra app standalone e plugin. Solo thread messaggi.
class FileLibrary
{
public:
    enum Kind { Nam = 0, Ir = 1 };
    static constexpr int kMaxRecent = 12;

    explicit FileLibrary (juce::File storageFile) : storage (std::move (storageFile)) { load(); }

    static FileLibrary& shared()
    {
        static FileLibrary lib (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                    .getChildFile ("DaniRig").getChildFile ("library.xml"));
        return lib;
    }

    void addRecent (Kind k, const juce::File& f)
    {
        auto& v = recents[k];
        v.removeString (f.getFullPathName());
        v.insert (0, f.getFullPathName());
        while (v.size() > kMaxRecent) v.remove (v.size() - 1);
        save();
    }
    void clearRecent (Kind k) { recents[k].clear(); save(); }
    juce::StringArray getRecent (Kind k) const { return recents[k]; }
    juce::StringArray getFavorites (Kind k) const { return favorites[k]; }
    bool isFavorite (Kind k, const juce::File& f) const { return favorites[k].contains (f.getFullPathName()); }
    void setFavorite (Kind k, const juce::File& f, bool fav)
    {
        favorites[k].removeString (f.getFullPathName());
        if (fav) favorites[k].add (f.getFullPathName());
        save();
    }

private:
    void load()
    {
        if (! storage.existsAsFile()) return;
        auto xml = juce::XmlDocument::parse (storage);
        if (xml == nullptr || ! xml->hasTagName ("Library")) return;
        for (auto* e : xml->getChildIterator())
        {
            const int k = e->getIntAttribute ("kind", -1);
            const auto path = e->getStringAttribute ("path");
            if (k < 0 || k > 1 || path.isEmpty()) continue;
            if (e->hasTagName ("Recent") && recents[k].size() < kMaxRecent) recents[k].addIfNotAlreadyThere (path);
            else if (e->hasTagName ("Favorite")) favorites[k].addIfNotAlreadyThere (path);
        }
    }
    void save() const
    {
        juce::XmlElement xml ("Library");
        for (int k = 0; k < 2; ++k)
        {
            for (auto& p : recents[k])   { auto* e = xml.createNewChildElement ("Recent");   e->setAttribute ("kind", k); e->setAttribute ("path", p); }
            for (auto& p : favorites[k]) { auto* e = xml.createNewChildElement ("Favorite"); e->setAttribute ("kind", k); e->setAttribute ("path", p); }
        }
        storage.getParentDirectory().createDirectory();
        xml.writeTo (storage);
    }

    juce::File storage;
    juce::StringArray recents[2], favorites[2];   // i recenti sono ordinati dal più recente
};
} // namespace dr
