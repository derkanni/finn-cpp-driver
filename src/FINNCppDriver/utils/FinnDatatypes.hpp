/**
 * @file FinnDatatypes.hpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Implements a (mostly) constexpr typesystem for FINN Datatypes
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef DATATYPE_H
#define DATATYPE_H

#include <FINNCppDriver/utils/FinnUtils.h>

#include <concepts>
#include <limits>
#include <type_traits>
#include <utility>

namespace Finn {
    /**
     * @brief Concept for an integral type
     *
     * @tparam T
     */
    template<class T>
    concept Integral = std::is_integral<T>::value;
    /**
     * @brief Concept for an signed integral type
     *
     * @tparam T
     */
    template<class T>
    concept SignedIntegral = Integral<T> && std::is_signed<T>::value;
    /**
     * @brief Concept for an unsigned integral type
     *
     * @tparam T
     */
    template<class T>
    concept UnsignedIntegral = Integral<T> && !SignedIntegral<T>;

    // forward declare Datatype for concept
    template<typename T>
    class Datatype;

    /**
     * @brief Concept to test if type T is subtype of Finn::Datatype
     *
     * @tparam T
     * @tparam D
     */
    template<typename T, typename D = T>
    concept IsDatatype = std::derived_from<T, Datatype<D>>;

    /**
     * @brief Represents Finn Datatypes
     *
     * @tparam D Subclass of Datatype (CRTP Pattern)
     */
    template<typename D>
    class Datatype {
         public:
        /**
         * @brief Query whether type is signed type.
         *
         * @return true Type is signed
         * @return false Type is unsigned
         */
        constexpr virtual bool sign() const = 0;

        /**
         * @brief Queries bitwidth of type
         *
         * @return constexpr std::size_t Bitwidth
         */
        constexpr virtual std::size_t bitwidth() const = 0;

        // /**
        //  * @brief Queries the bitwidth of data that can be stored in the type
        //  *
        //  * @return constexpr std::size_t
        //  */
        // constexpr virtual std::size_t dataBitwidth() const {
        //     if constexpr (sign()) {
        //         return bitwidth() - 1;
        //     } else {
        //         return bitwidth();
        //     }
        // }

        /**
         * @brief Minimum value that can be stored in the datatype
         *
         * @return constexpr double Value
         */
        constexpr virtual double min() const = 0;

        /**
         * @brief Maximum value that can be stored in the datatype
         *
         * @return constexpr double Value
         */
        constexpr virtual double max() const = 0;

        /**
         * @brief Test whether value is allowed in Datatype
         *
         * @tparam T Type of value
         * @param val Value to be tested
         * @return true Value is allowed
         * @return false Value is not allowed
         */
        template<Integral T>
        bool allowed(const T& val) const {
            return static_cast<const D*>(this)->template allowedImpl<T>(val);
        }

        /**
         * @brief Get the Number of Possible Values for Datatype
         *
         * @return constexpr double Number of possible values
         */
        constexpr virtual double getNumPossibleValues() const { return (min() < 0) ? -min() + max() + 1 : min() + max() + 1; }

        /**
         * @brief Test whether Datatype is integer type
         *
         * @return true Type is integer type
         * @return false Type is not an integer type
         */
        constexpr virtual bool isInteger() const = 0;

        /**
         * @brief Test whether Datatype is a fixed point type
         *
         * @return true Type is fixed point type
         * @return false Type is not a fixed point type
         */
        constexpr virtual bool isFixedPoint() const = 0;

        /**
         * @brief Get the number of elements of type T that are required to store one instance of the FINN datatype F.
         * For example to store a INT14 in uin8_t's, this function would return 2, since 2 uint8_t are required to store the INT14
         *
         * @tparam F
         * @tparam T
         * @return unsigned int
         */
        template<typename T>
        constexpr unsigned int requiredElements() const {
            // If the finn datatype has a lower bitwidth than the underlying machine datatype T, simply use one T per one Finn-Datatype
            if (bitwidth() < sizeof(T) * 8) {
                return 1;
            }
            // NOLINTNEXTLINE(clang-diagnostic-implicit-int-float-conversion)
            return static_cast<unsigned int>(FinnUtils::ceil(static_cast<float>(bitwidth()) / (static_cast<float>(sizeof(T)) * 8.0F)));
        }

        /**
         * @brief Comparison Operator equality
         *
         * @tparam D2 CRTP Type of other Datatype
         * @param lhs first Datatype to be compared against second Datatype
         * @param rhs second Datatype to be compared against first Datatype
         * @return true Both Datatypes are equal
         * @return false Datatypes are not equal
         */
        template<typename D2>
        constexpr friend bool operator==([[maybe_unused]] const Datatype<D>& lhs, [[maybe_unused]] const Datatype<D2>& rhs) {
            return std::is_same_v<D, D2>;
        }

        /**
         * @brief Comparison Operator inequality
         *
         * @tparam D2 CRTP Type of other Datatype
         * @param lhs first Datatype to be compared against second Datatype
         * @param rhs second Datatype to be compared against first Datatype
         * @return true Both Datatypes are not equal
         * @return false Datatypes are equal
         */
        template<typename D2>
        constexpr friend bool operator!=(const Datatype<D>& lhs, const Datatype<D2>& rhs) {
            return !(lhs == rhs);
        }

        /**
         * @brief Destroy the Datatype object
         *
         */
        constexpr virtual ~Datatype() = default;

         protected:
        /**
         * @brief Construct a new Datatype object (Move construction)
         *
         */
        Datatype(Datatype&&) noexcept = default;
        /**
         * @brief Construct a new Datatype object (Copy construction)
         *
         */
        Datatype(const Datatype&) = default;
        /**
         * @brief move assignment operator
         *
         * @return Datatype&
         */
        Datatype& operator=(Datatype&&) noexcept = default;
        /**
         * @brief copy assignment operator
         *
         * @return Datatype&
         */
        Datatype& operator=(const Datatype&) = default;

         private:
        /**
         * @brief Construct a new Datatype object; Some somewhat hacky code to make sure that CRTP is implemented correctly by all Derived classes -> creates error if for class A : public Base<B> A!=B
         *
         */
        Datatype() = default;
        friend D;

        /**
         * @brief Implementation of the allowed method. Is implemented by each subclass individually.
         *
         * @tparam T
         * @param val
         * @return true
         * @return false
         */
        template<typename T>
        bool static allowedImpl([[maybe_unused]] const T& val) {
            return false;
        }
    };

    /**
     * @brief Datatype to represent FP32 data
     *
     */
    class DatatypeFloat : public Datatype<DatatypeFloat> {
         private:
        /**
         * @brief Needed for CRTP
         *
         */
        friend class Datatype<DatatypeFloat>;

         public:
        /**
         * @brief Default constructor
         *
         */
        constexpr DatatypeFloat() = default;
        /**
         * @brief Destructor
         *
         */
        // NOLINTNEXTLINE
        constexpr ~DatatypeFloat() override{};  //{} instead of default because compiler bug
        /**
         * @brief Default move constructor
         *
         */
        constexpr DatatypeFloat(DatatypeFloat&&) noexcept = default;
        /**
         * @brief Default copy constructor
         *
         */
        constexpr DatatypeFloat(DatatypeFloat const&) = default;
        /**
         * @brief Default move assignment operator
         *
         * @return DatatypeFloat&
         */
        DatatypeFloat& operator=(DatatypeFloat&&) noexcept = default;
        /**
         * @brief Default copy assignment operator
         *
         * @return DatatypeFloat&
         */
        DatatypeFloat& operator=(const DatatypeFloat&) = default;

        /**
         * @brief @see Datatype
         */
        constexpr bool sign() const override { return true; }

        /**
         * @brief @see Datatype
         */
        constexpr std::size_t bitwidth() const override { return 32; }

        /**
         * @brief @see Datatype
         */
        constexpr double min() const override { return static_cast<double>(std::numeric_limits<float>::lowest()); }
        /**
         * @brief @see Datatype
         */
        constexpr double max() const override { return static_cast<double>(std::numeric_limits<float>::max()); }

        /**
         * @brief @see Datatype
         */
        constexpr bool isInteger() const override { return false; }

        /**
         * @brief @see Datatype
         */
        constexpr bool isFixedPoint() const override { return false; }

         private:
        /**
         * @brief Implementation of the allowed method. Is implemented by each subclass individually.
         *
         * @tparam T
         * @param val
         * @return true
         * @return false
         */
        template<typename T>
        [[maybe_unused]] bool allowedImpl(const T& val) const {
            return (val >= min()) && (val <= max());
        }
    };

    /**
     * @brief Datatype to represent signed int data
     *
     * @tparam B Bitwidth
     */
    template<std::size_t B>
    class DatatypeInt : public Datatype<DatatypeInt<B>> {
         private:
        /**
         * @brief Needed internally for CRTP
         *
         */
        friend class Datatype<DatatypeInt<B>>;

         public:
        /**
         * @brief Default Constructor
         *
         */
        constexpr DatatypeInt() = default;
        /**
         * @brief Default destructor
         *
         */
        // NOLINTNEXTLINE
        constexpr ~DatatypeInt() override{};  //{} instead of default because compiler bug
        /**
         * @brief Default move constructor
         *
         */
        constexpr DatatypeInt(DatatypeInt<B>&&) noexcept = default;
        /**
         * @brief Default copy constructor
         *
         */
        constexpr DatatypeInt(DatatypeInt<B> const&) = default;
        /**
         * @brief Default move assignment operator
         *
         * @return constexpr DatatypeInt<C>&
         */
        constexpr DatatypeInt<B>& operator=(DatatypeInt<B>&&) noexcept = default;
        /**
         * @brief Default copy assignment operator
         *
         * @return constexpr DatatypeInt<C>&
         */
        constexpr DatatypeInt<B>& operator=(const DatatypeInt<B>&) = default;

        /**
         * @brief @see Datatype
         */
        constexpr bool sign() const override { return true; }

        /**
         * @brief @see Datatype
         */
        constexpr std::size_t bitwidth() const override { return B; }

        /**
         * @brief @see Datatype
         */
        constexpr double min() const override { return -static_cast<double>(1UL << (B - 1)); }
        /**
         * @brief @see Datatype
         */
        constexpr double max() const override { return static_cast<double>((1UL << (B - 1)) - 1); }

        /**
         * @brief @see Datatype
         */
        constexpr bool isInteger() const override { return true; }
        /**
         * @brief @see Datatype
         */
        constexpr bool isFixedPoint() const override { return false; }

         private:
        /**
         * @brief Implementation of the allowed method. Is implemented by each subclass individually.
         *
         * @tparam T
         * @param val
         * @return true
         * @return false
         */
        template<typename T>
        [[maybe_unused]] bool allowedImpl(const T& val) const {
            return (val >= min()) && (val <= max());
        }
    };

    /**
     * @brief Datatype for fixed point data
     *
     * @tparam B Overall bitwidth
     * @tparam I Integer bitwidth
     */
    template<std::size_t B, std::size_t I>
    class DatatypeFixed : public Datatype<DatatypeFixed<B, I>> {
         private:
        /**
         * @brief Needed internally for CRTP
         *
         */
        friend class Datatype<DatatypeFixed<B, I>>;

         public:
        /**
         * @brief Default Constructor
         *
         */
        constexpr DatatypeFixed() = default;
        /**
         * @brief Default destructor
         *
         */
        // NOLINTNEXTLINE
        constexpr ~DatatypeFixed() override{};  //{} instead of default because compiler bug
        /**
         * @brief Default move constructor
         *
         */
        constexpr DatatypeFixed(DatatypeFixed<B, I>&&) noexcept = default;
        /**
         * @brief Default copy constructor
         *
         */
        constexpr DatatypeFixed(DatatypeFixed<B, I> const&) = default;
        /**
         * @brief Default move assignment operator
         *
         * @return constexpr DatatypeFixed<B, I>&
         */
        constexpr DatatypeFixed<B, I>& operator=(DatatypeFixed<B, I>&&) noexcept = default;
        /**
         * @brief Default copy assignment operator
         *
         * @return constexpr DatatypeFixed<B, I>&
         */
        constexpr DatatypeFixed<B, I>& operator=(const DatatypeFixed<B, I>&) = default;
        /**
         * @brief @see Datatype
         */
        constexpr bool sign() const override { return true; }

        /**
         * @brief @see Datatype
         */
        constexpr std::size_t bitwidth() const override { return B; }

        /**
         * @brief @see Datatype
         */
        constexpr std::size_t intBits() const { return I; }
        /**
         * @brief @see Datatype
         */
        constexpr std::size_t fracBits() const { return B - I; }

        /**
         * @brief @see Datatype
         */
        constexpr double scaleFactor() const { return 1.0 / (1U << fracBits()); }

        /**
         * @brief @see Datatype
         */
        constexpr double min() const override { return -static_cast<double>(1U << (B - 1)) * scaleFactor(); }
        /**
         * @brief @see Datatype
         */
        constexpr double max() const override { return ((1U << (B - 1)) - 1) * scaleFactor(); }

        /**
         * @brief @see Datatype
         */
        constexpr bool isInteger() const override { return false; }
        /**
         * @brief @see Datatype
         */
        constexpr bool isFixedPoint() const override { return true; }

         private:
        /**
         * @brief Implementation of the allowed method. Is implemented by each subclass individually.
         *
         * @tparam T
         * @param val
         * @return true
         * @return false
         */
        template<typename T>
        [[maybe_unused]] bool allowedImpl(const T& val) const {
            T intEquivalent = val * (1U << I);
            return (intEquivalent >= min()) && (intEquivalent <= max());
        }
    };

    /**
     * @brief Datatype for unsigned integers
     *
     * @tparam B Bitwidth
     */
    template<std::size_t B>
    class DatatypeUInt : public Datatype<DatatypeUInt<B>> {
         private:
        /**
         * @brief Needed internally for CRTP
         *
         */
        friend class Datatype<DatatypeUInt<B>>;

         public:
        /**
         * @brief Default Constructor
         *
         */
        constexpr DatatypeUInt() = default;
        // NOLINTNEXTLINE
        /**
         * @brief Default destructor
         *
         */
        constexpr ~DatatypeUInt() override{};  //{} instead of default because compiler bug
        /**
         * @brief Default move constructor
         *
         */
        constexpr DatatypeUInt(DatatypeUInt<B>&&) noexcept = default;
        /**
         * @brief Default copy constructor
         *
         */
        constexpr DatatypeUInt(DatatypeUInt<B> const&) = default;
        /**
         * @brief Default move assignment operator
         *
         * @return constexpr DatatypeUInt<C>&
         */
        constexpr DatatypeUInt<B>& operator=(DatatypeUInt<B>&&) noexcept = default;
        /**
         * @brief Default copy assignment operator
         *
         * @return constexpr DatatypeUInt<C>&
         */
        constexpr DatatypeUInt<B>& operator=(const DatatypeUInt<B>&) = default;

        /**
         * @brief @see Datatype
         */
        constexpr bool sign() const override { return false; }

        /**
         * @brief @see Datatype
         */
        constexpr std::size_t bitwidth() const override { return B; }

        /**
         * @brief @see Datatype
         */
        constexpr double min() const override { return 0; }
        /**
         * @brief @see Datatype
         */
        constexpr double max() const override { return static_cast<double>((static_cast<__uint128_t>(1U) << B) - 1); }

        /**
         * @brief @see Datatype
         */
        constexpr bool isInteger() const override { return true; }

        /**
         * @brief @see Datatype
         */
        constexpr bool isFixedPoint() const override { return false; }

         private:
        /**
         * @brief Implementation of the allowed method. Is implemented by each subclass individually.
         *
         * @tparam T
         * @param val
         * @return true
         * @return false
         */
        template<typename T>
        [[maybe_unused]] bool allowedImpl(const T& val) const {
            return (val >= min()) && (val <= max());
        }
    };

    /**
     * @brief Datatype for Binary data
     *
     */
    using DatatypeBinary = DatatypeUInt<1>;

    /**
     * @brief Datatype for Bipolar data
     *
     */
    class DatatypeBipolar : public Datatype<DatatypeBipolar> {
         private:
        /**
         * @brief Needed internally for CRTP
         *
         */
        friend class Datatype<DatatypeBipolar>;

         public:
        /**
         * @brief Default Constructor
         *
         */
        constexpr DatatypeBipolar() = default;
        /**
         * @brief Default destructor
         *
         */
        // NOLINTNEXTLINE
        constexpr ~DatatypeBipolar() override{};  //{} instead of default because compiler bug
        /**
         * @brief Default move constructor
         *
         */
        constexpr DatatypeBipolar(DatatypeBipolar&&) noexcept = default;
        /**
         * @brief Default copy constructor
         *
         */
        constexpr DatatypeBipolar(DatatypeBipolar const&) = default;
        /**
         * @brief Default move assignment operator
         *
         * @return DatatypeBipolar&
         */
        DatatypeBipolar& operator=(DatatypeBipolar&&) noexcept = default;
        /**
         * @brief Default copy assignment operator
         *
         * @return DatatypeBipolar&
         */
        DatatypeBipolar& operator=(const DatatypeBipolar&) = default;
        /**
         * @brief @see Datatype
         */
        constexpr bool sign() const override { return true; }

        /**
         * @brief @see Datatype
         */
        constexpr std::size_t bitwidth() const override { return 1; }

        /**
         * @brief @see Datatype
         */
        constexpr double min() const override { return -1; }
        /**
         * @brief @see Datatype
         */
        constexpr double max() const override { return 1; }

        /**
         * @brief @see Datatype
         */
        constexpr bool isInteger() const override { return true; }
        /**
         * @brief @see Datatype
         */
        constexpr bool isFixedPoint() const override { return false; }

        /**
         * @brief @see Datatype
         */
        constexpr double getNumPossibleValues() const override { return 2; }

         private:
        /**
         * @brief Implementation of the allowed method. Is implemented by each subclass individually.
         *
         * @tparam T
         * @param val
         * @return true
         * @return false
         */
        template<typename T>
        [[maybe_unused]] static bool allowedImpl(const T& val) {
            return (val == -1 || val == 1);
        }
    };

    /**
     * @brief Datatype to use with ternary data
     *
     */
    class DatatypeTernary : public Datatype<DatatypeTernary> {
         private:
        /**
         * @brief Needed internally for CRTP
         *
         */
        friend class Datatype<DatatypeTernary>;

         public:
        /**
         * @brief Default Constructor
         *
         */
        constexpr DatatypeTernary() = default;
        /**
         * @brief Default destructor
         *
         */
        // NOLINTNEXTLINE
        constexpr ~DatatypeTernary() override{};  //{} instead of default because compiler bug
        /**
         * @brief Default move constructor
         *
         */
        constexpr DatatypeTernary(DatatypeTernary&&) noexcept = default;
        /**
         * @brief Default copy constructor
         *
         */
        constexpr DatatypeTernary(DatatypeTernary const&) = default;
        /**
         * @brief Default move assignment operator
         *
         * @return DatatypeTernary&
         */
        DatatypeTernary& operator=(DatatypeTernary&&) noexcept = default;
        /**
         * @brief Defualt copy assignment operator
         *
         * @return DatatypeTernary&
         */
        DatatypeTernary& operator=(const DatatypeTernary&) = default;
        /**
         * @brief @see Datatype
         */
        constexpr bool sign() const override { return true; }

        /**
         * @brief @see Datatype
         */
        constexpr std::size_t bitwidth() const override { return 2; }

        /**
         * @brief @see Datatype
         */
        constexpr double min() const override { return -1; }
        /**
         * @brief @see Datatype
         */
        constexpr double max() const override { return 1; }

        /**
         * @brief @see Datatype
         */
        constexpr bool isInteger() const override { return true; }
        /**
         * @brief @see Datatype
         */
        constexpr bool isFixedPoint() const override { return false; }

        /**
         * @brief @see Datatype
         */
        constexpr double getNumPossibleValues() const override { return 3; }

         private:
        /**
         * @brief Implementation of the allowed method. Is implemented by each subclass individually.
         *
         * @tparam T
         * @param val
         * @return true
         * @return false
         */
        template<typename T>
        [[maybe_unused]] static bool allowedImpl(const T& val) {
            return (val == -1 || val == 1 || val == 0);
        }
    };
}  // namespace Finn

#endif  // DATATYPE_H