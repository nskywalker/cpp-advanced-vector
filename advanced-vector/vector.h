#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <execution>

template <typename T>
class RawMemory {
public:
    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept {
        Swap(other);
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    RawMemory() = default;

    explicit RawMemory(size_t capacity)
            : buffer_(Allocate(capacity))
            , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {

public:

    Vector(Vector&& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector temp(rhs);
                Swap(temp);
            }
            else {
                if (rhs.size_ < size_) {
                    for (size_t i = 0; i < rhs.size_; ++i) {
                        *(data_ + i) = *(rhs.data_ + i);
                    }
                    std::destroy_n(data_ + rhs.size_, size_ - rhs.size_);
                }
                else {
                    for (size_t i = 0; i < size_; ++i) {
                        *(data_ + i) = *(rhs.data_ + i);
                    }
                    std::uninitialized_copy_n(rhs.data_ + size_, rhs.size_ - size_, data_ + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }
    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    Vector() = default;

    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_ + new_size, size_ - new_size);
        }
        else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
        }
        size_ = new_size;
    }

    void PushBack(const T &value) {
        if (Capacity() == size_) {
            size_t new_size = (size_ == 0 ? 1 : size_ * 2);
            RawMemory<T> new_data(new_size);
            std::uninitialized_fill_n(new_data + size_, 1, value);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        else {
            std::uninitialized_fill_n(data_ + size_, 1, value);
        }
        ++size_;
    }
    void PushBack(T&& value) {
        if (Capacity() == size_) {
            size_t new_size = (size_ == 0 ? 1 : size_ * 2);
            RawMemory<T> new_data(new_size);
            new (&new_data[size_])T(std::move(value));
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        else {
            new (&data_[size_])T(std::move(value));
        }
        ++size_;
    }
    void PopBack() /* noexcept */ {
        std::destroy_n(data_ + size_ - 1, 1);
        --size_;
    }

    explicit Vector(size_t size)
            : data_(size)
            , size_(size)  //
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size_);
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    Vector(const Vector& other)
            : data_(other.size_)
            , size_(other.size_)  //
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (Capacity() == size_) {
            size_t new_size = (size_ == 0 ? 1 : size_ * 2);
            RawMemory<T> new_data(new_size);
            new (&new_data[size_])T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        else {
            new (&data_[size_])T(std::forward<Args>(args)...);
        }
        ++size_;
        return data_[size_ - 1];
    }

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return size_ == 0 ? data_.GetAddress() : data_ + size_;
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return size_ == 0 ? data_.GetAddress() : data_ + size_;
    }
    const_iterator cbegin() const noexcept {
        return begin();
    }
    const_iterator cend() const noexcept {
        return end();
    }

    template<typename... Args>
    iterator Emplace(const_iterator pos, Args &&... args) {
        int index = pos ? pos - begin() : 0;
        if (Capacity() == size_) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data + index)T(std::forward<Args>(args)...);
            if (size_ != 0) {
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    std::uninitialized_move_n(data_.GetAddress(), index, new_data.GetAddress());
                    std::uninitialized_move_n(data_ + index, size_ - index, new_data + index + 1);
                } else {
                    std::uninitialized_copy_n(data_.GetAddress(), index, new_data.GetAddress());
                    std::uninitialized_copy_n(data_ + index, size_ - index, new_data + index + 1);
                }
                std::destroy_n(data_.GetAddress(), size_);
            }
            data_.Swap(new_data);
        }
        else {
            if (size_ != 0) {
                std::uninitialized_move_n(data_ + size_ - 1, 1, data_ + size_);
                std::move_backward(data_ + index, end() - 1, end());
                data_[index] = std::move(T(std::forward<Args>(args)...));
            }
            else {
                new (data_ + index)T(std::forward<Args>(args)...);
            }
        }
        ++size_;
        return data_ + index;
    }
    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/ {
        int index = pos ? pos - begin() : 0;
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            for (auto it = begin() + index + 1; it != end(); ++it) {
                *(it - 1) = std::move(*it);
            }

        } else {
            for (auto it = begin() + index + 1; it != end(); ++it) {
                *(it - 1) = (*it);
            }
        }
        std::destroy_n(data_ + size_ - 1, 1);
        --size_;
        return data_ + index;
    }
    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    iterator Insert(const_iterator pos, T&& value) {

        return Emplace(pos, std::move(value));
    }
private:
    RawMemory<T> data_;
    size_t size_ = 0;
};