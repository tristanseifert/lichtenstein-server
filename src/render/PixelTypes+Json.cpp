#include "HSIPixel.h"
#include "RGBPixel.h"
#include "RGBWPixel.h"

#include <nlohmann/json.hpp>

using namespace Lichtenstein::Server::Render;
using json = nlohmann::json;



/**
 * Attempts to extract an HSI pixel value from the provided JSON.
 */
void from_json(const nlohmann::json &j, HSIPixel &p) {
    j.at("h").get_to(p.h);
    j.at("s").get_to(p.s);
    j.at("i").get_to(p.i);
}
/**
 * Converts an HSI pixel to JSON.
 */
void to_json(nlohmann::json &j, const HSIPixel &p) {
    j = {{"h", p.h}, {"s", p.s}, {"i", p.i}};
}


/**
 * Attempts to extract an RGB pixel value from the provided JSON.
 */
void from_json(const nlohmann::json &j, RGBPixel &p) {
    j.at("r").get_to(p.r);
    j.at("g").get_to(p.g);
    j.at("b").get_to(p.b);
}
/**
 * Converts an RGB pixel to JSON.
 */
void to_json(nlohmann::json &j, const RGBPixel &p) {
    j = {{"r", p.r}, {"g", p.g}, {"b", p.b}};
}


/**
 * Attempts to extract an RGBW pixel value from the provided JSON.
 */
void from_json(const nlohmann::json &j, RGBWPixel &p) {
    j.at("r").get_to(p.r);
    j.at("g").get_to(p.g);
    j.at("b").get_to(p.b);
    j.at("w").get_to(p.w);
}
/**
 * Converts an RGBW pixel to JSON.
 */
void to_json(nlohmann::json &j, const RGBWPixel &p) {
    j = {{"r", p.r}, {"g", p.g}, {"b", p.b}, {"w", p.w}};
}

