#include "GenericCircuit.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace circuitpedal {
namespace {

constexpr double minimumResistance = 1.0e-3;
constexpr double minimumCapacitance = 1.0e-15;
constexpr double minimumThermalVoltage = 1.0e-6;
constexpr double maximumExponentialArgument = 30.0;
constexpr double pivotTolerance = 1.0e-18;
constexpr std::size_t maximumUnknowns = 128;

bool finitePositive(double value, double minimum = 0.0) noexcept
{
    return std::isfinite(value) && value > minimum;
}

struct ExponentialJunction {
    double current = 0.0;
    double conductance = 0.0;
};

struct FetChannelEvaluation {
    double current = 0.0;
    double dCurrent_dDrain = 0.0;
    double dCurrent_dGate = 0.0;
    double dCurrent_dSource = 0.0;
};

FetChannelEvaluation forwardNjfetChannel(double drainVolts,
                                         double gateVolts,
                                         double sourceVolts,
                                         const GenericNjfetModel& model) noexcept
{
    const double vds = drainVolts - sourceVolts;
    const double vgs = gateVolts - sourceVolts;
    if (vds < 0.0)
        return {};

    const double pinch = model.pinchOffVoltageVolts;
    const double overdrive = vgs - pinch;
    if (overdrive <= 0.0)
        return {};

    const double beta = 2.0 * model.idssAmps / (pinch * pinch);
    double current = 0.0;
    double gm = 0.0;
    double gds = 0.0;

    if (vds < overdrive)
    {
        current = beta * (overdrive * vds - 0.5 * vds * vds);
        gm = beta * vds;
        gds = beta * (overdrive - vds);
    }
    else
    {
        current = 0.5 * beta * overdrive * overdrive;
        gm = beta * overdrive;
        gds = 0.0;
    }

    return {
        current,
        gds,
        gm,
        -(gds + gm)
    };
}

FetChannelEvaluation evaluateNjfetChannel(double drainVolts,
                                          double gateVolts,
                                          double sourceVolts,
                                          const GenericNjfetModel& model) noexcept
{
    if (drainVolts >= sourceVolts)
        return forwardNjfetChannel(drainVolts, gateVolts, sourceVolts, model);

    const auto reverse =
        forwardNjfetChannel(sourceVolts, gateVolts, drainVolts, model);
    // reverse describes current S->D. Convert it to the requested D->S current.
    return {
        -reverse.current,
        -reverse.dCurrent_dSource,
        -reverse.dCurrent_dGate,
        -reverse.dCurrent_dDrain
    };
}

ExponentialJunction exponentialJunction(double voltage,
                                        double saturationCurrent,
                                        double scaleVoltage) noexcept
{
    if (!(saturationCurrent > 0.0))
        return {};

    const double safeScale = std::max(scaleVoltage, minimumThermalVoltage);
    const double raw = voltage / safeScale;
    const double limited = std::clamp(raw,
                                      -maximumExponentialArgument,
                                      maximumExponentialArgument);
    const double exponential = std::exp(limited);

    // Continue the exponential linearly outside the bounded evaluation region.
    // This preserves a consistent current/derivative pair for Newton iteration.
    const double currentScale =
        exponential - 1.0 + exponential * (raw - limited);
    return {
        saturationCurrent * currentScale,
        saturationCurrent * exponential / safeScale
    };
}

} // namespace

CircuitDefinition::CircuitDefinition()
{
    nodeNames_.push_back("0");
}

CircuitNode CircuitDefinition::addNode(const std::string& name)
{
    const CircuitNode existing = findNode(name);
    if (existing != std::numeric_limits<CircuitNode>::max())
        return existing;

    nodeNames_.push_back(name);
    return static_cast<CircuitNode>(nodeNames_.size() - 1);
}

CircuitNode CircuitDefinition::findNode(const std::string& name) const noexcept
{
    for (std::size_t i = 0; i < nodeNames_.size(); ++i)
    {
        if (nodeNames_[i] == name)
            return static_cast<CircuitNode>(i);
    }
    return std::numeric_limits<CircuitNode>::max();
}

const std::string& CircuitDefinition::nodeName(CircuitNode node) const noexcept
{
    static const std::string unknown = "?";
    return node < nodeNames_.size() ? nodeNames_[node] : unknown;
}

void CircuitDefinition::addResistor(CircuitNode a,
                                    CircuitNode b,
                                    double resistanceOhms)
{
    resistors_.push_back({ a, b, resistanceOhms });
}

void CircuitDefinition::addCapacitor(CircuitNode a,
                                     CircuitNode b,
                                     double capacitanceFarads)
{
    capacitors_.push_back({ a, b, capacitanceFarads });
}

void CircuitDefinition::addVoltageSource(CircuitNode positive,
                                         CircuitNode negative,
                                         double dcVolts,
                                         double audioScaleVoltsPerFullScale)
{
    voltageSources_.push_back({
        positive, negative, dcVolts, audioScaleVoltsPerFullScale
    });
}

void CircuitDefinition::addDiode(CircuitNode anode,
                                 CircuitNode cathode,
                                 const GenericDiodeModel& model)
{
    diodes_.push_back({ anode, cathode, model });
}

void CircuitDefinition::addNpnBjt(CircuitNode collector,
                                  CircuitNode base,
                                  CircuitNode emitter,
                                  const GenericNpnBjtModel& model)
{
    npnBjts_.push_back({ collector, base, emitter, model });
}

void CircuitDefinition::addPnpBjt(CircuitNode collector,
                                  CircuitNode base,
                                  CircuitNode emitter,
                                  const GenericPnpBjtModel& model)
{
    pnpBjts_.push_back({ collector, base, emitter, model });
}

void CircuitDefinition::addNjfet(CircuitNode drain,
                                 CircuitNode gate,
                                 CircuitNode source,
                                 const GenericNjfetModel& model)
{
    njfets_.push_back({ drain, gate, source, model });
}

std::size_t CircuitDefinition::addPotentiometer(CircuitNode terminal1,
                                                CircuitNode wiper,
                                                CircuitNode terminal3,
                                                double totalResistanceOhms,
                                                double position,
                                                double taperExponent)
{
    potentiometers_.push_back({
        terminal1,
        wiper,
        terminal3,
        totalResistanceOhms,
        position,
        taperExponent
    });
    return potentiometers_.size() - 1;
}

bool CircuitDefinition::setPotentiometerPosition(std::size_t index,
                                                 double normalized) noexcept
{
    if (index >= potentiometers_.size() || !std::isfinite(normalized))
        return false;
    potentiometers_[index].position = std::clamp(normalized, 0.0, 1.0);
    return true;
}

double CircuitDefinition::potentiometerPosition(std::size_t index) const noexcept
{
    return index < potentiometers_.size() ? potentiometers_[index].position : 0.0;
}

bool CircuitDefinition::validate(std::string& error) const
{
    error.clear();
    if (nodeNames_.empty() || nodeNames_[0] != "0")
    {
        error = "Circuit ground node is missing.";
        return false;
    }
    if (nodeNames_.size() > maximumUnknowns)
    {
        error = "Circuit has too many nodes for the current real-time solver.";
        return false;
    }

    const auto nodeValid = [this](CircuitNode node) {
        return node < nodeNames_.size();
    };
    const auto pairValid = [&nodeValid](CircuitNode a, CircuitNode b) {
        return nodeValid(a) && nodeValid(b) && a != b;
    };

    for (const auto& component : resistors_)
    {
        if (!pairValid(component.a, component.b)
            || !finitePositive(component.resistanceOhms, minimumResistance))
        {
            error = "Circuit contains an invalid resistor.";
            return false;
        }
    }
    for (const auto& component : capacitors_)
    {
        if (!pairValid(component.a, component.b)
            || !finitePositive(component.capacitanceFarads, minimumCapacitance))
        {
            error = "Circuit contains an invalid capacitor.";
            return false;
        }
    }
    for (const auto& source : voltageSources_)
    {
        if (!pairValid(source.positive, source.negative)
            || !std::isfinite(source.dcVolts)
            || !std::isfinite(source.audioScaleVoltsPerFullScale))
        {
            error = "Circuit contains an invalid voltage source.";
            return false;
        }
    }
    for (const auto& diode : diodes_)
    {
        if (!pairValid(diode.anode, diode.cathode)
            || !finitePositive(diode.model.idealityFactor)
            || !finitePositive(diode.model.thermalVoltageVolts)
            || !std::isfinite(diode.model.saturationCurrentAmps)
            || diode.model.saturationCurrentAmps < 0.0)
        {
            error = "Circuit contains an invalid diode model.";
            return false;
        }
    }
    for (const auto& transistor : npnBjts_)
    {
        if (!nodeValid(transistor.collector)
            || !nodeValid(transistor.base)
            || !nodeValid(transistor.emitter)
            || transistor.collector == transistor.base
            || transistor.collector == transistor.emitter
            || transistor.base == transistor.emitter
            || !finitePositive(transistor.model.saturationCurrentAmps)
            || !finitePositive(transistor.model.forwardBeta)
            || !finitePositive(transistor.model.reverseBeta)
            || !finitePositive(transistor.model.emissionCoefficient)
            || !finitePositive(transistor.model.thermalVoltageVolts))
        {
            error = "Circuit contains an invalid NPN BJT model.";
            return false;
        }
    }
    for (const auto& transistor : pnpBjts_)
    {
        if (!nodeValid(transistor.collector)
            || !nodeValid(transistor.base)
            || !nodeValid(transistor.emitter)
            || transistor.collector == transistor.base
            || transistor.collector == transistor.emitter
            || transistor.base == transistor.emitter
            || !finitePositive(transistor.model.saturationCurrentAmps)
            || !finitePositive(transistor.model.forwardBeta)
            || !finitePositive(transistor.model.reverseBeta)
            || !finitePositive(transistor.model.emissionCoefficient)
            || !finitePositive(transistor.model.thermalVoltageVolts))
        {
            error = "Circuit contains an invalid PNP BJT model.";
            return false;
        }
    }
    for (const auto& transistor : njfets_)
    {
        if (!nodeValid(transistor.drain)
            || !nodeValid(transistor.gate)
            || !nodeValid(transistor.source)
            || transistor.drain == transistor.gate
            || transistor.drain == transistor.source
            || transistor.gate == transistor.source
            || !finitePositive(transistor.model.idssAmps)
            || !std::isfinite(transistor.model.pinchOffVoltageVolts)
            || transistor.model.pinchOffVoltageVolts >= -1.0e-6
            || !std::isfinite(transistor.model.gateSaturationCurrentAmps)
            || transistor.model.gateSaturationCurrentAmps < 0.0
            || !finitePositive(transistor.model.gateIdealityFactor)
            || !finitePositive(transistor.model.thermalVoltageVolts))
        {
            error = "Circuit contains an invalid N-JFET model.";
            return false;
        }
    }
    for (const auto& pot : potentiometers_)
    {
        if (!nodeValid(pot.terminal1)
            || !nodeValid(pot.wiper)
            || !nodeValid(pot.terminal3)
            || !finitePositive(pot.totalResistanceOhms, minimumResistance)
            || !std::isfinite(pot.position)
            || pot.position < 0.0
            || pot.position > 1.0
            || !finitePositive(pot.taperExponent))
        {
            error = "Circuit contains an invalid potentiometer.";
            return false;
        }
    }
    if (!nodeValid(outputNode_))
    {
        error = "Circuit output node is invalid.";
        return false;
    }
    if (!finitePositive(outputFullScalePerVolt_))
    {
        error = "Circuit output calibration is invalid.";
        return false;
    }

    const std::size_t unknowns =
        (nodeNames_.size() - 1) + voltageSources_.size();
    if (unknowns == 0 || unknowns > maximumUnknowns)
    {
        error = "Circuit MNA system size is unsupported.";
        return false;
    }
    return true;
}

bool GenericCircuit::compile(const CircuitDefinition& definition,
                             double sampleRate,
                             std::string& error)
{
    compiled_ = false;
    lastSolveConverged_ = false;
    error.clear();

    if (!definition.validate(error))
        return false;
    if (!std::isfinite(sampleRate) || sampleRate < 8000.0 || sampleRate > 384000.0)
    {
        error = "Generic circuit sample rate is unsupported.";
        return false;
    }

    nodeNames_ = definition.nodeNames_;
    resistors_ = definition.resistors_;
    capacitors_.clear();
    capacitors_.reserve(definition.capacitors_.size());
    for (const auto& capacitor : definition.capacitors_)
        capacitors_.push_back({ capacitor, 0.0 });
    voltageSources_ = definition.voltageSources_;
    diodes_ = definition.diodes_;
    npnBjts_ = definition.npnBjts_;
    pnpBjts_ = definition.pnpBjts_;
    njfets_ = definition.njfets_;
    potentiometers_ = definition.potentiometers_;
    if (potentiometers_.size() > maximumLivePotentiometers)
    {
        error = "Circuit has too many live potentiometers.";
        return false;
    }
    potentiometerTargetCount_ = potentiometers_.size();
    for (std::size_t i = 0; i < potentiometerTargetCount_; ++i)
    {
        potentiometerTargets_[i].store(
            static_cast<float>(potentiometers_[i].position),
            std::memory_order_relaxed);
    }
    outputNode_ = definition.outputNode_;
    outputFullScalePerVolt_ = definition.outputFullScalePerVolt_;

    sampleRate_ = sampleRate;
    timestep_ = 1.0 / sampleRate_;
    nodeUnknownCount_ = nodeNames_.size() - 1;
    unknownCount_ = nodeUnknownCount_ + voltageSources_.size();

    solution_.assign(unknownCount_, 0.0);
    dcSolution_.assign(unknownCount_, 0.0);
    lastGoodSolution_.assign(unknownCount_, 0.0);
    residual_.assign(unknownCount_, 0.0);
    jacobian_.assign(unknownCount_ * unknownCount_, 0.0);
    workMatrix_.assign(unknownCount_ * unknownCount_, 0.0);
    workRhs_.assign(unknownCount_, 0.0);
    delta_.assign(unknownCount_, 0.0);

    compiled_ = true;
    if (!solveOperatingPoint())
    {
        compiled_ = false;
        error = "Generic circuit DC operating-point solve did not converge. "
                "Check topology, grounding and transistor bias.";
        return false;
    }

    dcSolution_ = solution_;
    lastGoodSolution_ = solution_;
    for (auto& capacitor : capacitors_)
    {
        capacitor.previousVoltage =
            voltage(capacitor.component.a) - voltage(capacitor.component.b);
    }
    lastSolveConverged_ = true;
    return true;
}

void GenericCircuit::reset() noexcept
{
    if (!compiled_)
        return;

    std::copy(dcSolution_.begin(), dcSolution_.end(), solution_.begin());
    std::copy(dcSolution_.begin(), dcSolution_.end(), lastGoodSolution_.begin());
    for (auto& capacitor : capacitors_)
    {
        capacitor.previousVoltage =
            voltage(capacitor.component.a) - voltage(capacitor.component.b);
    }
    lastSolveConverged_ = true;
}

float GenericCircuit::processSample(float input) noexcept
{
    if (!compiled_ || !std::isfinite(input))
        return 0.0f;

    lastSolveConverged_ = solveTransient(static_cast<double>(input));
    if (!lastSolveConverged_)
    {
        std::copy(lastGoodSolution_.begin(), lastGoodSolution_.end(), solution_.begin());
        return 0.0f;
    }

    std::copy(solution_.begin(), solution_.end(), lastGoodSolution_.begin());
    const double output = voltage(outputNode_) * outputFullScalePerVolt_;
    if (!std::isfinite(output))
        return 0.0f;
    return static_cast<float>(std::clamp(output, -1.0, 1.0));
}

double GenericCircuit::nodeVoltage(CircuitNode node) const noexcept
{
    return compiled_ ? voltage(node) : 0.0;
}

bool GenericCircuit::setPotentiometerPosition(std::size_t index,
                                              double normalized) noexcept
{
    if (index >= potentiometerTargetCount_ || !std::isfinite(normalized))
        return false;
    potentiometerTargets_[index].store(
        static_cast<float>(std::clamp(normalized, 0.0, 1.0)),
        std::memory_order_relaxed);
    return true;
}

double GenericCircuit::potentiometerPosition(std::size_t index) const noexcept
{
    return index < potentiometerTargetCount_
        ? static_cast<double>(
              potentiometerTargets_[index].load(std::memory_order_relaxed))
        : 0.0;
}

bool GenericCircuit::solveOperatingPoint() noexcept
{
    std::fill(solution_.begin(), solution_.end(), 0.0);
    const bool converged = newtonSolve(true, 0.0);
    return converged;
}

bool GenericCircuit::solveTransient(double input) noexcept
{
    if (!newtonSolve(false, input))
        return false;

    for (auto& capacitor : capacitors_)
    {
        capacitor.previousVoltage =
            voltage(capacitor.component.a) - voltage(capacitor.component.b);
    }
    return true;
}

bool GenericCircuit::newtonSolve(bool dcMode, double input) noexcept
{
    constexpr int maximumIterations = 40;
    constexpr double nodeStepLimitVolts = 0.35;
    constexpr double nodeDeltaTolerance = 1.0e-8;
    constexpr double currentResidualTolerance = 1.0e-8;
    constexpr double voltageResidualTolerance = 1.0e-8;

    for (int iteration = 0; iteration < maximumIterations; ++iteration)
    {
        clearSystem();
        stampLinear(dcMode, input);
        stampNonlinear();

        double maximumNodeResidual = 0.0;
        for (std::size_t i = 0; i < nodeUnknownCount_; ++i)
            maximumNodeResidual = std::max(maximumNodeResidual, std::abs(residual_[i]));

        double maximumSourceResidual = 0.0;
        for (std::size_t i = nodeUnknownCount_; i < unknownCount_; ++i)
            maximumSourceResidual = std::max(maximumSourceResidual, std::abs(residual_[i]));

        if (maximumNodeResidual <= currentResidualTolerance
            && maximumSourceResidual <= voltageResidualTolerance)
        {
            return true;
        }

        if (!solveLinearSystem())
            return false;

        double maximumNodeDelta = 0.0;
        for (std::size_t i = 0; i < nodeUnknownCount_; ++i)
            maximumNodeDelta = std::max(maximumNodeDelta, std::abs(delta_[i]));

        double damping = 1.0;
        if (maximumNodeDelta > nodeStepLimitVolts)
            damping = nodeStepLimitVolts / maximumNodeDelta;

        maximumNodeDelta = 0.0;
        for (std::size_t i = 0; i < unknownCount_; ++i)
        {
            const double change = delta_[i] * damping;
            solution_[i] += change;
            if (!std::isfinite(solution_[i]))
                return false;
            if (i < nodeUnknownCount_)
                maximumNodeDelta = std::max(maximumNodeDelta, std::abs(change));
        }

        if (maximumNodeDelta <= nodeDeltaTolerance)
        {
            clearSystem();
            stampLinear(dcMode, input);
            stampNonlinear();

            double nodeResidual = 0.0;
            for (std::size_t i = 0; i < nodeUnknownCount_; ++i)
                nodeResidual = std::max(nodeResidual, std::abs(residual_[i]));
            double sourceResidual = 0.0;
            for (std::size_t i = nodeUnknownCount_; i < unknownCount_; ++i)
                sourceResidual = std::max(sourceResidual, std::abs(residual_[i]));
            return nodeResidual <= currentResidualTolerance
                && sourceResidual <= voltageResidualTolerance;
        }
    }
    return false;
}

void GenericCircuit::clearSystem() noexcept
{
    std::fill(residual_.begin(), residual_.end(), 0.0);
    std::fill(jacobian_.begin(), jacobian_.end(), 0.0);
}

void GenericCircuit::stampLinear(bool dcMode, double input) noexcept
{
    for (const auto& resistor : resistors_)
    {
        const double conductance = 1.0 / resistor.resistanceOhms;
        stampConductance(resistor.a, resistor.b, conductance);
        stampCurrent(resistor.a,
                     resistor.b,
                     conductance * (voltage(resistor.a) - voltage(resistor.b)));
    }

    for (std::size_t potIndex = 0; potIndex < potentiometers_.size(); ++potIndex)
    {
        const auto& pot = potentiometers_[potIndex];
        const double livePosition = static_cast<double>(
            potentiometerTargets_[potIndex].load(std::memory_order_relaxed));
        const double fraction =
            std::pow(std::clamp(livePosition, 0.0, 1.0), pot.taperExponent);
        const double r1 = std::max(minimumResistance,
                                   pot.totalResistanceOhms * fraction);
        const double r3 = std::max(minimumResistance,
                                   pot.totalResistanceOhms * (1.0 - fraction));

        const double g1 = 1.0 / r1;
        const double g3 = 1.0 / r3;
        stampConductance(pot.terminal1, pot.wiper, g1);
        stampCurrent(pot.terminal1,
                     pot.wiper,
                     g1 * (voltage(pot.terminal1) - voltage(pot.wiper)));
        stampConductance(pot.wiper, pot.terminal3, g3);
        stampCurrent(pot.wiper,
                     pot.terminal3,
                     g3 * (voltage(pot.wiper) - voltage(pot.terminal3)));
    }

    if (!dcMode)
    {
        for (const auto& capacitor : capacitors_)
        {
            const double conductance =
                capacitor.component.capacitanceFarads / timestep_;
            const double current = conductance
                * ((voltage(capacitor.component.a) - voltage(capacitor.component.b))
                   - capacitor.previousVoltage);
            stampConductance(capacitor.component.a,
                             capacitor.component.b,
                             conductance);
            stampCurrent(capacitor.component.a,
                         capacitor.component.b,
                         current);
        }
    }

    for (std::size_t sourceIndex = 0;
         sourceIndex < voltageSources_.size();
         ++sourceIndex)
    {
        const auto& source = voltageSources_[sourceIndex];
        const std::size_t branchIndex = nodeUnknownCount_ + sourceIndex;
        const int positive = nodeIndex(source.positive);
        const int negative = nodeIndex(source.negative);
        const double sourceValue =
            source.dcVolts + (dcMode ? 0.0 : input * source.audioScaleVoltsPerFullScale);
        const double branchCurrent = solution_[branchIndex];

        if (positive >= 0)
        {
            residual_[static_cast<std::size_t>(positive)] += branchCurrent;
            jacobian_[static_cast<std::size_t>(positive) * unknownCount_ + branchIndex] += 1.0;
            jacobian_[branchIndex * unknownCount_ + static_cast<std::size_t>(positive)] += 1.0;
        }
        if (negative >= 0)
        {
            residual_[static_cast<std::size_t>(negative)] -= branchCurrent;
            jacobian_[static_cast<std::size_t>(negative) * unknownCount_ + branchIndex] -= 1.0;
            jacobian_[branchIndex * unknownCount_ + static_cast<std::size_t>(negative)] -= 1.0;
        }

        residual_[branchIndex] +=
            voltage(source.positive) - voltage(source.negative) - sourceValue;
    }
}

void GenericCircuit::stampNonlinear() noexcept
{
    for (const auto& diode : diodes_)
    {
        const double diodeVoltage = voltage(diode.anode) - voltage(diode.cathode);
        const auto junction = exponentialJunction(
            diodeVoltage,
            diode.model.saturationCurrentAmps,
            diode.model.idealityFactor * diode.model.thermalVoltageVolts);

        stampCurrent(diode.anode, diode.cathode, junction.current);
        stampConductance(diode.anode, diode.cathode, junction.conductance);
    }

    for (const auto& transistor : npnBjts_)
    {
        const auto& model = transistor.model;
        const double alphaForward = model.forwardBeta / (model.forwardBeta + 1.0);
        const double alphaReverse = model.reverseBeta / (model.reverseBeta + 1.0);
        const double junctionScale =
            model.emissionCoefficient * model.thermalVoltageVolts;

        const double vbe = voltage(transistor.base) - voltage(transistor.emitter);
        const double vbc = voltage(transistor.base) - voltage(transistor.collector);
        const auto be = exponentialJunction(vbe,
                                            model.saturationCurrentAmps,
                                            junctionScale);
        const auto bc = exponentialJunction(vbc,
                                            model.saturationCurrentAmps,
                                            junctionScale);

        const double collectorCurrent = alphaForward * be.current - bc.current;
        const double baseCurrent =
            (1.0 - alphaForward) * be.current
            + (1.0 - alphaReverse) * bc.current;
        const double emitterCurrent = -be.current + alphaReverse * bc.current;

        const int c = nodeIndex(transistor.collector);
        const int b = nodeIndex(transistor.base);
        const int e = nodeIndex(transistor.emitter);

        if (c >= 0)
            residual_[static_cast<std::size_t>(c)] += collectorCurrent;
        if (b >= 0)
            residual_[static_cast<std::size_t>(b)] += baseCurrent;
        if (e >= 0)
            residual_[static_cast<std::size_t>(e)] += emitterCurrent;

        const double dIc_dB = alphaForward * be.conductance - bc.conductance;
        const double dIc_dC = bc.conductance;
        const double dIc_dE = -alphaForward * be.conductance;

        const double dIb_dB =
            (1.0 - alphaForward) * be.conductance
            + (1.0 - alphaReverse) * bc.conductance;
        const double dIb_dC = -(1.0 - alphaReverse) * bc.conductance;
        const double dIb_dE = -(1.0 - alphaForward) * be.conductance;

        const double dIe_dB = -be.conductance + alphaReverse * bc.conductance;
        const double dIe_dC = -alphaReverse * bc.conductance;
        const double dIe_dE = be.conductance;

        stampJacobianCurrent(transistor.collector, transistor.base, dIc_dB);
        stampJacobianCurrent(transistor.collector, transistor.collector, dIc_dC);
        stampJacobianCurrent(transistor.collector, transistor.emitter, dIc_dE);

        stampJacobianCurrent(transistor.base, transistor.base, dIb_dB);
        stampJacobianCurrent(transistor.base, transistor.collector, dIb_dC);
        stampJacobianCurrent(transistor.base, transistor.emitter, dIb_dE);

        stampJacobianCurrent(transistor.emitter, transistor.base, dIe_dB);
        stampJacobianCurrent(transistor.emitter, transistor.collector, dIe_dC);
        stampJacobianCurrent(transistor.emitter, transistor.emitter, dIe_dE);
    }
}

void GenericCircuit::stampConductance(CircuitNode a,
                                      CircuitNode b,
                                      double conductance) noexcept
{
    const int ai = nodeIndex(a);
    const int bi = nodeIndex(b);
    if (ai >= 0)
    {
        const std::size_t aIndex = static_cast<std::size_t>(ai);
        jacobian_[aIndex * unknownCount_ + aIndex] += conductance;
        if (bi >= 0)
            jacobian_[aIndex * unknownCount_ + static_cast<std::size_t>(bi)] -= conductance;
    }
    if (bi >= 0)
    {
        const std::size_t bIndex = static_cast<std::size_t>(bi);
        jacobian_[bIndex * unknownCount_ + bIndex] += conductance;
        if (ai >= 0)
            jacobian_[bIndex * unknownCount_ + static_cast<std::size_t>(ai)] -= conductance;
    }
}

void GenericCircuit::stampCurrent(CircuitNode a,
                                  CircuitNode b,
                                  double current) noexcept
{
    const int ai = nodeIndex(a);
    const int bi = nodeIndex(b);
    if (ai >= 0)
        residual_[static_cast<std::size_t>(ai)] += current;
    if (bi >= 0)
        residual_[static_cast<std::size_t>(bi)] -= current;
}

void GenericCircuit::stampJacobianCurrent(CircuitNode rowNode,
                                          CircuitNode columnNode,
                                          double derivative) noexcept
{
    const int row = nodeIndex(rowNode);
    const int column = nodeIndex(columnNode);
    if (row >= 0 && column >= 0)
    {
        jacobian_[static_cast<std::size_t>(row) * unknownCount_
                  + static_cast<std::size_t>(column)] += derivative;
    }
}

bool GenericCircuit::solveLinearSystem() noexcept
{
    std::copy(jacobian_.begin(), jacobian_.end(), workMatrix_.begin());
    for (std::size_t i = 0; i < unknownCount_; ++i)
        workRhs_[i] = -residual_[i];

    for (std::size_t column = 0; column < unknownCount_; ++column)
    {
        std::size_t pivot = column;
        double pivotMagnitude =
            std::abs(workMatrix_[pivot * unknownCount_ + column]);
        for (std::size_t row = column + 1; row < unknownCount_; ++row)
        {
            const double candidate =
                std::abs(workMatrix_[row * unknownCount_ + column]);
            if (candidate > pivotMagnitude)
            {
                pivot = row;
                pivotMagnitude = candidate;
            }
        }

        if (!std::isfinite(pivotMagnitude) || pivotMagnitude < pivotTolerance)
            return false;

        if (pivot != column)
        {
            for (std::size_t j = column; j < unknownCount_; ++j)
            {
                std::swap(workMatrix_[column * unknownCount_ + j],
                          workMatrix_[pivot * unknownCount_ + j]);
            }
            std::swap(workRhs_[column], workRhs_[pivot]);
        }

        const double diagonal = workMatrix_[column * unknownCount_ + column];
        for (std::size_t row = column + 1; row < unknownCount_; ++row)
        {
            const double factor =
                workMatrix_[row * unknownCount_ + column] / diagonal;
            if (factor == 0.0)
                continue;
            workMatrix_[row * unknownCount_ + column] = 0.0;
            for (std::size_t j = column + 1; j < unknownCount_; ++j)
            {
                workMatrix_[row * unknownCount_ + j] -=
                    factor * workMatrix_[column * unknownCount_ + j];
            }
            workRhs_[row] -= factor * workRhs_[column];
        }
    }

    for (std::size_t reverse = 0; reverse < unknownCount_; ++reverse)
    {
        const std::size_t row = unknownCount_ - 1 - reverse;
        double value = workRhs_[row];
        for (std::size_t column = row + 1; column < unknownCount_; ++column)
            value -= workMatrix_[row * unknownCount_ + column] * delta_[column];

        const double diagonal = workMatrix_[row * unknownCount_ + row];
        if (!std::isfinite(diagonal) || std::abs(diagonal) < pivotTolerance)
            return false;
        delta_[row] = value / diagonal;
        if (!std::isfinite(delta_[row]))
            return false;
    }
    return true;
}

double GenericCircuit::voltage(CircuitNode node) const noexcept
{
    const int index = nodeIndex(node);
    return index >= 0 ? solution_[static_cast<std::size_t>(index)] : 0.0;
}

int GenericCircuit::nodeIndex(CircuitNode node) const noexcept
{
    if (node == circuitGround)
        return -1;
    if (node >= nodeNames_.size())
        return -1;
    return static_cast<int>(node - 1);
}

} // namespace circuitpedal
