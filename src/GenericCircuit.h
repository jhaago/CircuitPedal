#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace circuitpedal {

using CircuitNode = std::uint32_t;
constexpr CircuitNode circuitGround = 0;

struct GenericDiodeModel {
    double saturationCurrentAmps = 1.0e-12;
    double idealityFactor = 1.8;
    double thermalVoltageVolts = 0.02585;
};

struct GenericNpnBjtModel {
    // Compact Ebers-Moll parameters. Named devices remain approximations until
    // the later SPICE/Gummel-Poon device-library pass.
    double saturationCurrentAmps = 6.0e-15;
    double forwardBeta = 180.0;
    double reverseBeta = 4.0;
    double emissionCoefficient = 1.0;
    double thermalVoltageVolts = 0.02585;
};

struct GenericPnpBjtModel {
    double saturationCurrentAmps = 6.0e-15;
    double forwardBeta = 120.0;
    double reverseBeta = 4.0;
    double emissionCoefficient = 1.0;
    double thermalVoltageVolts = 0.02585;
};

struct GenericNjfetModel {
    // Compact Shichman-Hodges/JFET channel model. pinchOffVoltageVolts is
    // negative for an N-channel depletion device.
    double idssAmps = 3.0e-3;
    double pinchOffVoltageVolts = -2.0;
    double gateSaturationCurrentAmps = 1.0e-14;
    double gateIdealityFactor = 1.2;
    double thermalVoltageVolts = 0.02585;
};

struct CircuitResistor {
    CircuitNode a = circuitGround;
    CircuitNode b = circuitGround;
    double resistanceOhms = 1000.0;
};

struct CircuitCapacitor {
    CircuitNode a = circuitGround;
    CircuitNode b = circuitGround;
    double capacitanceFarads = 1.0e-9;
};

struct CircuitVoltageSource {
    CircuitNode positive = circuitGround;
    CircuitNode negative = circuitGround;
    double dcVolts = 0.0;
    // The current audio sample is multiplied by this value and added to dcVolts.
    // Use 0 for a fixed supply and e.g. 1.0 for a 1 Vpk/full-scale input source.
    double audioScaleVoltsPerFullScale = 0.0;
};

struct CircuitDiode {
    CircuitNode anode = circuitGround;
    CircuitNode cathode = circuitGround;
    GenericDiodeModel model;
};

struct CircuitNpnBjt {
    CircuitNode collector = circuitGround;
    CircuitNode base = circuitGround;
    CircuitNode emitter = circuitGround;
    GenericNpnBjtModel model;
};

struct CircuitPnpBjt {
    CircuitNode collector = circuitGround;
    CircuitNode base = circuitGround;
    CircuitNode emitter = circuitGround;
    GenericPnpBjtModel model;
};

struct CircuitNjfet {
    CircuitNode drain = circuitGround;
    CircuitNode gate = circuitGround;
    CircuitNode source = circuitGround;
    GenericNjfetModel model;
};

struct CircuitPotentiometer {
    CircuitNode terminal1 = circuitGround;
    CircuitNode wiper = circuitGround;
    CircuitNode terminal3 = circuitGround;
    double totalResistanceOhms = 10000.0;
    double position = 0.5;       // 0 = wiper at terminal1, 1 = terminal3.
    double taperExponent = 1.0;  // 1 = linear.
};

class CircuitDefinition {
public:
    CircuitDefinition();

    CircuitNode addNode(const std::string& name);
    CircuitNode findNode(const std::string& name) const noexcept;
    const std::string& nodeName(CircuitNode node) const noexcept;
    std::size_t nodeCount() const noexcept { return nodeNames_.size(); }

    void addResistor(CircuitNode a, CircuitNode b, double resistanceOhms);
    void addCapacitor(CircuitNode a, CircuitNode b, double capacitanceFarads);
    void addVoltageSource(CircuitNode positive,
                          CircuitNode negative,
                          double dcVolts,
                          double audioScaleVoltsPerFullScale = 0.0);
    void addDiode(CircuitNode anode,
                  CircuitNode cathode,
                  const GenericDiodeModel& model = {});
    void addNpnBjt(CircuitNode collector,
                   CircuitNode base,
                   CircuitNode emitter,
                   const GenericNpnBjtModel& model = {});
    void addPnpBjt(CircuitNode collector,
                   CircuitNode base,
                   CircuitNode emitter,
                   const GenericPnpBjtModel& model = {});
    void addNjfet(CircuitNode drain,
                  CircuitNode gate,
                  CircuitNode source,
                  const GenericNjfetModel& model = {});
    std::size_t addPotentiometer(CircuitNode terminal1,
                                 CircuitNode wiper,
                                 CircuitNode terminal3,
                                 double totalResistanceOhms,
                                 double position = 0.5,
                                 double taperExponent = 1.0);
    bool setPotentiometerPosition(std::size_t index, double normalized) noexcept;
    double potentiometerPosition(std::size_t index) const noexcept;
    std::size_t potentiometerCount() const noexcept { return potentiometers_.size(); }

    void setOutputNode(CircuitNode node) noexcept { outputNode_ = node; }
    CircuitNode outputNode() const noexcept { return outputNode_; }
    void setOutputFullScalePerVolt(double scale) noexcept { outputFullScalePerVolt_ = scale; }
    double outputFullScalePerVolt() const noexcept { return outputFullScalePerVolt_; }

    bool validate(std::string& error) const;

private:
    friend class GenericCircuit;

    std::vector<std::string> nodeNames_;
    std::vector<CircuitResistor> resistors_;
    std::vector<CircuitCapacitor> capacitors_;
    std::vector<CircuitVoltageSource> voltageSources_;
    std::vector<CircuitDiode> diodes_;
    std::vector<CircuitNpnBjt> npnBjts_;
    std::vector<CircuitPnpBjt> pnpBjts_;
    std::vector<CircuitNjfet> njfets_;
    std::vector<CircuitPotentiometer> potentiometers_;
    CircuitNode outputNode_ = circuitGround;
    double outputFullScalePerVolt_ = 1.0;
};

// Dense-MNA circuit processor intended for small analogue pedal circuits.
// Topology compilation and DC operating-point work may allocate. processSample()
// performs no heap allocation, locking or console I/O.
class GenericCircuit {
public:
    static constexpr std::size_t maximumLivePotentiometers = 16;
    bool compile(const CircuitDefinition& definition,
                 double sampleRate,
                 std::string& error);
    void reset() noexcept;

    float processSample(float input) noexcept;
    double nodeVoltage(CircuitNode node) const noexcept;
    bool lastSolveConverged() const noexcept { return lastSolveConverged_; }

    std::size_t potentiometerCount() const noexcept { return potentiometers_.size(); }
    bool setPotentiometerPosition(std::size_t index, double normalized) noexcept;
    double potentiometerPosition(std::size_t index) const noexcept;

    double sampleRate() const noexcept { return sampleRate_; }
    std::size_t unknownCount() const noexcept { return unknownCount_; }

private:
    struct RuntimeCapacitor {
        CircuitCapacitor component;
        double previousVoltage = 0.0;
    };

    bool solveOperatingPoint() noexcept;
    bool solveTransient(double input) noexcept;
    bool newtonSolve(bool dcMode, double input) noexcept;
    bool solveLinearSystem() noexcept;
    void clearSystem() noexcept;
    void stampLinear(bool dcMode, double input) noexcept;
    void stampNonlinear() noexcept;
    void stampConductance(CircuitNode a, CircuitNode b, double conductance) noexcept;
    void stampCurrent(CircuitNode a, CircuitNode b, double current) noexcept;
    void stampJacobianCurrent(CircuitNode rowNode,
                              CircuitNode columnNode,
                              double derivative) noexcept;
    double voltage(CircuitNode node) const noexcept;
    int nodeIndex(CircuitNode node) const noexcept;

    std::vector<std::string> nodeNames_;
    std::vector<CircuitResistor> resistors_;
    std::vector<RuntimeCapacitor> capacitors_;
    std::vector<CircuitVoltageSource> voltageSources_;
    std::vector<CircuitDiode> diodes_;
    std::vector<CircuitNpnBjt> npnBjts_;
    std::vector<CircuitPnpBjt> pnpBjts_;
    std::vector<CircuitNjfet> njfets_;
    std::vector<CircuitPotentiometer> potentiometers_;
    std::array<std::atomic<float>, maximumLivePotentiometers> potentiometerTargets_ {};
    std::size_t potentiometerTargetCount_ = 0;

    CircuitNode outputNode_ = circuitGround;
    double outputFullScalePerVolt_ = 1.0;
    double sampleRate_ = 48000.0;
    double timestep_ = 1.0 / 48000.0;

    std::size_t nodeUnknownCount_ = 0;
    std::size_t unknownCount_ = 0;

    std::vector<double> solution_;
    std::vector<double> dcSolution_;
    std::vector<double> lastGoodSolution_;
    std::vector<double> residual_;
    std::vector<double> jacobian_;
    std::vector<double> workMatrix_;
    std::vector<double> workRhs_;
    std::vector<double> delta_;

    bool compiled_ = false;
    bool lastSolveConverged_ = false;
};

} // namespace circuitpedal
