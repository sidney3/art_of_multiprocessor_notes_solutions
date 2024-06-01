import math
class TerminalWire:
    def __init__(self, name):
        self.count = 0
        self.name = name
    def pushToken(self):
        self.count += 1
        # print(f'Balancer {self.name} received a token')

class IntermediateBalancer:
    def __init__(self, upConnection, downConnection):
        self.upConnection = upConnection
        self.downConnection = downConnection
        self.switchUp = True

    def pushToken(self):
        if self.switchUp:
            self.upConnection.pushToken()
            self.switchUp = False
        else:
            self.downConnection.pushToken()
            self.switchUp = True 
class SourceWire:
    def __init__(self, Balancer):
        self.balancer = Balancer
    def pushToken(self):
        self.balancer.pushToken()


class Bitonic4:
    def __init__(self):

        self.terminals = [TerminalWire("1"), TerminalWire("2"), TerminalWire("3"), TerminalWire("4")]

        self.i5, self.i6 = IntermediateBalancer(self.terminals[0], self.terminals[1]), IntermediateBalancer(self.terminals[2], self.terminals[3])
        self.i3, self.i4 = IntermediateBalancer(self.i5, self.i6), IntermediateBalancer(self.i5, self.i6)
        self.i1, self.i2 = IntermediateBalancer(self.i3, self.i4), IntermediateBalancer(self.i4, self.i3)
        
        self.sources = [SourceWire(self.i1), SourceWire(self.i1), SourceWire(self.i2), SourceWire(self.i2)]
    def pushToWire(self, wireIndex):
        self.sources[wireIndex].pushToken()
    def queryTerminals(self):
        res = [0] * len(self.terminals)
        for i,term in enumerate(self.terminals):
            res[i] = term.count
        return res

def allCombinations(List, CombinationSize):
    if CombinationSize == 0:
        return [[]]

    res = []
    for combo in allCombinations(List, CombinationSize - 1):
        for elt in List:
            comboCopy = combo.copy()
            comboCopy.append(elt)
            res.append(comboCopy)
    return res

def hasStepProperty(freqs):
    total_tokens = sum(freqs)
    num_wires = len(freqs)

    res = True
    for wire, wire_tokens in enumerate(freqs):
        expected_tokens = math.ceil((total_tokens - wire) / num_wires)
        res = res and wire_tokens == expected_tokens

    return res

def main():
    assert(hasStepProperty([1,1,1,1]))
    assert(hasStepProperty([1,1,1,0]))
    assert(hasStepProperty([1,1,0,0]))
    assert(hasStepProperty([1,0,0,0]))
    assert(hasStepProperty([0,0,0,0]))

    for numTokens in range(1, 9):
        for combo in allCombinations(range(4), numTokens):
            network = Bitonic4()
            for wire in combo:
                network.pushToWire(wire)
            assert(hasStepProperty(network.queryTerminals()))

if __name__ == "__main__":
    main()



