/**
 * \copyright
 * Copyright (c) 2012-2019, OpenGeoSys Community (http://www.opengeosys.org)
 *            Distributed under a Modified BSD License.
 *              See accompanying file LICENSE.txt or
 *              http://www.opengeosys.org/project/license
 *
 */

#pragma once

#include <Eigen/Eigen>

#include "BaseLib/Error.h"

#include "BHECommon.h"
#include "FlowAndTemperatureControl.h"
#include "PipeConfiguration1U.h"

namespace ProcessLib
{
namespace HeatTransportBHE
{
namespace BHE
{
/**
 * The BHE_1U class is the realization of 1U type of Borehole Heate Exchanger.
 * In this class, the pipe heat capacity, pipe heat conductiion, pie advection
 * vectors are intialized according to the geometry of 1U type of BHE.
 * For 1U type of BHE, 4 primary unknowns are assigned on the 1D BHE elements.
 * They are the temperature in inflow pipe T_in, temperature in outflow pipe
 * T_out temperature of the two grout zones sorrounding the inflow and outflow
 * pipe T_g1 and T_g2. These primary varaibles are solved according to heat
 * convection and conduction equations on the pipes and also in the grout zones.
 * The interaction of the 1U type of BHE and the sorrounding soil is regulated
 * through the thermal resistance values, which are calculated specifically
 * during the initialization of the class.
 */
class BHE_1U final : public BHECommon
{
public:
    BHE_1U(BoreholeGeometry const& borehole,
           RefrigerantProperties const& refrigerant,
           GroutParameters const& grout,
           FlowAndTemperatureControl const& flowAndTemperatureControl,
           PipeConfiguration1U const& pipes);

    static constexpr int number_of_unknowns = 4;

    std::array<double, number_of_unknowns> pipeHeatCapacities() const;

    std::array<double, number_of_unknowns> pipeHeatConductions() const;

    std::array<Eigen::Vector3d, number_of_unknowns> pipeAdvectionVectors()
        const;

    template <int NPoints,
              typename SingleUnknownMatrixType,
              typename RMatrixType,
              typename RPiSMatrixType,
              typename RSMatrixType>
    void assembleRMatrices(
        int const idx_bhe_unknowns,
        Eigen::MatrixBase<SingleUnknownMatrixType> const& matBHE_loc_R,
        Eigen::MatrixBase<RMatrixType>& R_matrix,
        Eigen::MatrixBase<RPiSMatrixType>& R_pi_s_matrix,
        Eigen::MatrixBase<RSMatrixType>& R_s_matrix) const
    {
        switch (idx_bhe_unknowns)
        {
            case 0:  // PHI_fig
                R_matrix.block(0, 2 * NPoints, NPoints, NPoints) +=
                    -1.0 * matBHE_loc_R;
                R_matrix.block(2 * NPoints, 0, NPoints, NPoints) +=
                    -1.0 * matBHE_loc_R;

                R_matrix.block(0, 0, NPoints, NPoints) +=
                    1.0 * matBHE_loc_R;  // K_i1
                R_matrix.block(2 * NPoints, 2 * NPoints, NPoints, NPoints) +=
                    1.0 * matBHE_loc_R;  // K_ig
                return;
            case 1:  // PHI_fog
                R_matrix.block(NPoints, 3 * NPoints, NPoints, NPoints) +=
                    -1.0 * matBHE_loc_R;
                R_matrix.block(3 * NPoints, NPoints, NPoints, NPoints) +=
                    -1.0 * matBHE_loc_R;

                R_matrix.block(NPoints, NPoints, NPoints, NPoints) +=
                    1.0 * matBHE_loc_R;  // K_o1
                R_matrix.block(3 * NPoints, 3 * NPoints, NPoints, NPoints) +=
                    1.0 * matBHE_loc_R;  // K_og
                return;
            case 2:  // PHI_gg
                R_matrix.block(2 * NPoints, 3 * NPoints, NPoints, NPoints) +=
                    -1.0 * matBHE_loc_R;
                R_matrix.block(3 * NPoints, 2 * NPoints, NPoints, NPoints) +=
                    -1.0 * matBHE_loc_R;

                R_matrix.block(2 * NPoints, 2 * NPoints, NPoints, NPoints) +=
                    1.0 * matBHE_loc_R;  // K_ig  // notice we only have
                                         // 1 PHI_gg term here.
                R_matrix.block(3 * NPoints, 3 * NPoints, NPoints, NPoints) +=
                    1.0 * matBHE_loc_R;  // K_og  // see Diersch 2013 FEFLOW
                                         // book page 954 Table M.2
                return;
            case 3:  // PHI_gs
                R_s_matrix.template block<NPoints, NPoints>(0, 0).noalias() +=
                    1.0 * matBHE_loc_R;

                R_pi_s_matrix.block(2 * NPoints, 0, NPoints, NPoints) +=
                    -1.0 * matBHE_loc_R;
                R_pi_s_matrix.block(3 * NPoints, 0, NPoints, NPoints) +=
                    -1.0 * matBHE_loc_R;
                R_matrix.block(2 * NPoints, 2 * NPoints, NPoints, NPoints) +=
                    1.0 * matBHE_loc_R;  // K_ig
                R_matrix.block(3 * NPoints, 3 * NPoints, NPoints, NPoints) +=
                    1.0 * matBHE_loc_R;  // K_og
                return;
            default:
                OGS_FATAL(
                    "Error!!! In the function BHE_1U::assembleRMatrices, "
                    "the index of bhe unknowns is out of range! ");
        }
    }

    /// Return the inflow temperature for the boundary condition.
    double updateFlowRateAndTemperature(double T_out, double current_time);

    double thermalResistance(int const unknown_index) const
    {
        return _thermal_resistances[unknown_index];
    }

    static constexpr std::pair<int, int> inflow_outflow_bc_component_ids[] = {
        {0, 1}};

private:
    // Placing it here before using it in the cross_section_areas.
    PipeConfiguration1U const _pipes;

public:
    std::array<double, number_of_unknowns> const cross_section_areas = {
        {_pipes.inlet.area(), _pipes.outlet.area(),
         borehole_geometry.area() / 2 - _pipes.inlet.area(),
         borehole_geometry.area() / 2 - _pipes.outlet.area()}};

private:
    void updateHeatTransferCoefficients(double const flow_rate);

    std::array<double, number_of_unknowns> calcThermalResistances(
        double const Nu);

private:
    /// PHI_fig, PHI_fog, PHI_gg, PHI_gs;
    /// Here we store the thermal resistances needed for computation of the heat
    /// exchange coefficients in the governing equations of BHE.
    /// These governing equations can be found in
    /// 1) Diersch (2013) FEFLOW book on page 958, M.3, or
    /// 2) Diersch (2011) Comp & Geosci 37:1122-1135, Eq. 90-97.
    std::array<double, number_of_unknowns> _thermal_resistances;

    /// Flow velocity inside the pipes. Depends on the flow_rate.
    double _flow_velocity;
};
}  // namespace BHE
}  // namespace HeatTransportBHE
}  // namespace ProcessLib
