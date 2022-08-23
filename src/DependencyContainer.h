#pragma once

#include <cassert>
#include <memory>
#include <vector>

template <typename ContainerType>
class DependencyContainer final
{
public:
	DependencyContainer() = default;
	DependencyContainer(DependencyContainer&) = delete;
	DependencyContainer(DependencyContainer&& other) : mDependencies(std::move(other.mDependencies))
	{}

	~DependencyContainer()
	{
		for (auto it = mDependencies.rbegin(); it != mDependencies.rend(); ++it) {
			it->reset();
		}
	}

	template <typename DependencyType, typename... Args>
	DependencyType& CreateDependency(Args&&... args)
	{
		size_t id = DependencyID<DependencyType>::ID();
		assert(id == mDependencies.size());
		auto& added =
			mDependencies.emplace_back(std::make_shared<DependencyType>(std::forward<Args>(args)...));
		return *static_cast<DependencyType*>(added.get());
	}

	template <typename DependencyType>
	void AddDependency(const std::shared_ptr<DependencyType>& existingDependency)
	{
		size_t id = DependencyID<DependencyType>::ID();
		assert(id == mDependencies.size());
		mDependencies.push_back(existingDependency);
	}

	template <typename DependencyType, typename... Args>
	DependencyType& GetDependency() const
	{
		size_t id = DependencyID<std::remove_const<DependencyType>::type>::ID();
		assert(id < mDependencies.size());
		return *static_cast<DependencyType*>(mDependencies[id].get());
	}

	template <typename DependencyType, typename ReceivingContainerType>
	DependencyType& ShareDependencyWith(DependencyContainer<ReceivingContainerType>& receivingContainer) const
	{
		size_t id = DependencyID<DependencyType>::ID();
		assert(id < mDependencies.size());
		const std::shared_ptr<DependencyType>& dependency =
			std::static_pointer_cast<DependencyType>(mDependencies[id]);
		receivingContainer.AddDependency<DependencyType>(dependency);
		return *dependency;
	}

private:
	struct DependencyIDType
	{
		static size_t NextID()
		{
			static size_t value = -1;
			value += 1;
			return value;
		}
	};

	template <typename DependencType>
	struct DependencyID
	{
		static size_t ID()
		{
			static const size_t id = DependencyIDType::NextID();
			return id;
		}
	};

	std::vector<std::shared_ptr<void>> mDependencies;
};
