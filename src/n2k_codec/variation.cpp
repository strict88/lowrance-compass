#include "variation.h"

#include "thresholds.h"

namespace n2k_codec
{

bool resolveVariation(const VariationSource &source, float &variation_rad_out)
{
    if (source.bus_ever_received && source.bus_age_s <= thresholds::kStageCReferenceMaxAgeS)
    {
        variation_rad_out = source.bus_variation_rad;
        return true;
    }
    if (source.manual_available)
    {
        variation_rad_out = source.manual_variation_rad;
        return true;
    }
    return false;
}

}  // namespace n2k_codec
