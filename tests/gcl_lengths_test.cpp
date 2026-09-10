#include "mgo2win/gcl_lengths.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <initializer_list>
#include <vector>
using namespace mgo2win::gcl;

void check(bool value) {
    if (!value) { std::cerr << "GCL contract mismatch\n"; std::exit(1); }
}
void expect(bool block, std::initializer_list<std::uint8_t> input,
            std::uint32_t value, std::size_t size) {
    const std::vector<std::uint8_t> bytes(input);
    const auto actual = block ? block_length(bytes) : command_length(bytes);
    check(actual && actual->value == value && actual->header_bytes == size);
}
int main() {
    // Asymmetric values catch the historical block-endianness documentation error.
    expect(true, {0x3e,0x34,0x12}, 0x1234,3);
    expect(true, {0x6f,0x56,0x34,0x12}, 0x123456,4);
    expect(true, {0x7d,0xff},255,2);
    expect(true, {0x0f,0xff,0xff,0xff},0xffffff,4);
    expect(false,{0x81,0x23},0x123,2);
    expect(false,{0xff,0xff},0x7fff,2);
    expect(false,{0x80,0x00},0,2);
    // Every inline tag with every opcode high nibble; high nibble must be ignored.
    for (unsigned hi=0;hi<16;++hi)
        for (unsigned lo=0;lo<13;++lo) {
            const std::array<std::uint8_t,1> input{static_cast<std::uint8_t>((hi<<4)|lo)};
            const auto r=block_length(input);
            check(r && r->value==lo && r->header_bytes==1);
        }
    for (unsigned value=0;value<128;++value)
        expect(false,{static_cast<std::uint8_t>(value)},value,1);
    check(!block_length({}) && !command_length({}));
    for (std::uint8_t tag=13;tag<=15;++tag) {
        const std::array<std::uint8_t,4> input{tag,1,2,3};
        for (std::size_t n=1;n<static_cast<std::size_t>(tag-11);++n)
            check(!block_length(std::span(input).first(n)));
    }
    for (unsigned first=128;first<256;++first) {
        const std::array<std::uint8_t,1> input{static_cast<std::uint8_t>(first)};
        check(!command_length(input));
    }
    std::cout << "GCL length contracts: endian, opcode tags, boundaries and truncation passed\n";
}
